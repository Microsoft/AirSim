// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AdHocConnectionImpl.hpp"
#include "Utils.hpp"
#include "ThreadUtils.hpp"
#include "../serial_com/Port.h"
#include "../serial_com/SerialPort.hpp"
#include "../serial_com/UdpClientPort.hpp"
#include "../serial_com/TcpClientPort.hpp"

using namespace mavlink_utils;
using namespace mavlinkcom_impl;

AdHocConnectionImpl::AdHocConnectionImpl()
{
    closed = true;
    ::memset(&mavlink_intermediate_status_, 0, sizeof(mavlink_status_t));
    ::memset(&mavlink_status_, 0, sizeof(mavlink_status_t));
}
std::string AdHocConnectionImpl::getName()
{
    return name;
}

AdHocConnectionImpl::~AdHocConnectionImpl()
{
    con_.reset();
    close();
}

std::shared_ptr<AdHocConnection> AdHocConnectionImpl::createConnection(const std::string& nodeName, std::shared_ptr<Port> port)
{
    // std::shared_ptr<MavLinkCom> owner, const std::string& nodeName
    std::shared_ptr<AdHocConnection> con = std::make_shared<AdHocConnection>();
    con->startListening(nodeName, port);
    return con;
}

std::shared_ptr<AdHocConnection> AdHocConnectionImpl::connectLocalUdp(const std::string& nodeName, std::string localAddr, int localPort)
{
    std::shared_ptr<UdpClientPort> socket = std::make_shared<UdpClientPort>();

    socket->connect(localAddr, localPort, "", 0);

    return createConnection(nodeName, socket);
}

std::shared_ptr<AdHocConnection> AdHocConnectionImpl::connectRemoteUdp(const std::string& nodeName, std::string localAddr, std::string remoteAddr, int remotePort)
{
    std::string local = localAddr;
    // just a little sanity check on the local address, if remoteAddr is localhost then localAddr must be also.
    if (remoteAddr == "127.0.0.1") {
        local = "127.0.0.1";
    }

    std::shared_ptr<UdpClientPort> socket = std::make_shared<UdpClientPort>();

    socket->connect(local, 0, remoteAddr, remotePort);

    return createConnection(nodeName, socket);
}

std::shared_ptr<AdHocConnection> AdHocConnectionImpl::connectTcp(const std::string& nodeName, std::string localAddr, const std::string& remoteIpAddr, int remotePort)
{
    std::string local = localAddr;
    // just a little sanity check on the local address, if remoteAddr is localhost then localAddr must be also.
    if (remoteIpAddr == "127.0.0.1") {
        local = "127.0.0.1";
    }

    std::shared_ptr<TcpClientPort> socket = std::make_shared<TcpClientPort>();

    socket->connect(local, 0, remoteIpAddr, remotePort);

    return createConnection(nodeName, socket);
}

std::shared_ptr<AdHocConnection> AdHocConnectionImpl::connectSerial(const std::string& nodeName, std::string name, int baudRate, const std::string initString)
{
    std::shared_ptr<SerialPort> serial = std::make_shared<SerialPort>();

    int hr = serial->connect(name.c_str(), baudRate);
    if (hr != 0)
        throw std::runtime_error(Utils::stringf("Could not open the serial port %s, error=%d", name.c_str(), hr));

    // send this right away just in case serial link is not already configured
    if (initString.size() > 0) {
        serial->write(reinterpret_cast<const uint8_t*>(initString.c_str()), static_cast<int>(initString.size()));
    }

    return createConnection(nodeName, serial);
}

void AdHocConnectionImpl::startListening(std::shared_ptr<AdHocConnection> parent, const std::string& nodeName, std::shared_ptr<Port> connectedPort)
{
    name = nodeName;
    con_ = parent;
    close();
    closed = false;
    port = connectedPort;

    Utils::cleanupThread(read_thread);
    read_thread = std::thread{ &AdHocConnectionImpl::readPackets, this };
    Utils::cleanupThread(publish_thread_);
    publish_thread_ = std::thread{ &AdHocConnectionImpl::publishPackets, this };
}

void AdHocConnectionImpl::close()
{
    closed = true;
    if (port != nullptr) {
        port->close();
        port = nullptr;
    }

    if (read_thread.joinable()) {
        read_thread.join();
    }
    if (publish_thread_.joinable()) {
        msg_available_.post();
        publish_thread_.join();
    }
}

bool AdHocConnectionImpl::isOpen()
{
    return !closed;
}

int AdHocConnectionImpl::getTargetComponentId()
{
    return this->other_component_id;
}
int AdHocConnectionImpl::getTargetSystemId()
{
    return this->other_system_id;
}

void AdHocConnectionImpl::sendMessage(const std::vector<uint8_t>& msg)
{
    if (closed) {
        return;
    }

    try {
        port->write(msg.data(), static_cast<int>(msg.size()));
    }
    catch (std::exception& e) {
        throw std::runtime_error(Utils::stringf("AdHocConnectionImpl: Error sending message on connection '%s', details: %s", name.c_str(), e.what()));
    }
}

int AdHocConnectionImpl::subscribe(AdHocMessageHandler handler)
{
    MessageHandlerEntry entry = { static_cast<int>(listeners.size() + 1), handler = handler };
    std::lock_guard<std::mutex> guard(listener_mutex);
    listeners.push_back(entry);
    snapshot_stale = true;
    return entry.id;
}
void AdHocConnectionImpl::unsubscribe(int id)
{
    std::lock_guard<std::mutex> guard(listener_mutex);
    for (auto ptr = listeners.begin(), end = listeners.end(); ptr != end; ptr++) {
        if ((*ptr).id == id) {
            listeners.erase(ptr);
            snapshot_stale = true;
            break;
        }
    }
}

void AdHocConnectionImpl::readPackets()
{
    //CurrentThread::setMaximumPriority();
    CurrentThread::setThreadName("MavLinkThread");
    std::shared_ptr<Port> safePort = this->port;
    const int MAXBUFFER = 512;
    uint8_t* buffer = new uint8_t[MAXBUFFER];
    int channel = 0;
    int hr = 0;
    while (hr == 0 && con_ != nullptr && !closed) {
        int read = 0;
        if (safePort->isClosed()) {
            // hmmm, wait till it is opened?
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int count = safePort->read(buffer, MAXBUFFER);
        if (count <= 0) {
            // error? well let's try again, but we should be careful not to spin too fast and kill the CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if (count >= MAXBUFFER) {

            std::cerr << "GAH KM911 message size (" << std::to_string(count) << ") is bigger than max buffer size! Time to support frame breaks, Moffitt" << std::endl;

            // error? well let's try again, but we should be careful not to spin too fast and kill the CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // queue event for publishing.
        {
            std::lock_guard<std::mutex> guard(msg_queue_mutex_);
            std::vector<uint8_t> message(count);
            memcpy(message.data(), buffer, count);
            msg_queue_.push(message);
        }

        if (waiting_for_msg_) {
            msg_available_.post();
        }

    } //while

    delete[] buffer;

} //readPackets

void AdHocConnectionImpl::drainQueue()
{
    std::vector<uint8_t> message;
    bool hasMsg = true;
    while (hasMsg) {
        hasMsg = false;
        {
            std::lock_guard<std::mutex> guard(msg_queue_mutex_);
            if (!msg_queue_.empty()) {
                message = msg_queue_.front();
                msg_queue_.pop();
                hasMsg = true;
            }
        }
        if (!hasMsg) {
            return;
        }
        // publish the message from this thread, this is safer than publishing from the readPackets thread
        // as it ensures we don't lose messages if the listener is slow.
        if (snapshot_stale) {
            // this is tricky, the clear has to be done outside the lock because it is destructing the handlers
            // and the handler might try and call unsubscribe, which needs to be able to grab the lock, otherwise
            // we would get a deadlock.
            snapshot.clear();

            std::lock_guard<std::mutex> guard(listener_mutex);
            snapshot = listeners;
            snapshot_stale = false;
        }
        auto end = snapshot.end();

        auto startTime = std::chrono::system_clock::now();
        std::shared_ptr<AdHocConnection> sharedPtr = std::shared_ptr<AdHocConnection>(this->con_);
        for (auto ptr = snapshot.begin(); ptr != end; ptr++) {
            try {
                (*ptr).handler(sharedPtr, message);
            }
            catch (std::exception& e) {
                Utils::log(Utils::stringf("AdHocConnectionImpl: Error handling message on connection '%s', details: %s",
                                          name.c_str(),
                                          e.what()),
                           Utils::kLogLevelError);
            }
        }
    }
}

void AdHocConnectionImpl::publishPackets()
{
    //CurrentThread::setMaximumPriority();
    CurrentThread::setThreadName("MavLinkThread");
    while (!closed) {

        drainQueue();

        waiting_for_msg_ = true;
        msg_available_.wait();
        waiting_for_msg_ = false;
    }
}
