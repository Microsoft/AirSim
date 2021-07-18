// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkMessages.hpp"
#include "MavLinkConnectionImpl.hpp"
#include "Utils.hpp"
#include "ThreadUtils.hpp"
#include "../serial_com/Port.h"
#include "../serial_com/SerialPort.hpp"
#include "../serial_com/UdpClientPort.hpp"
#include "../serial_com/TcpClientPort.hpp"

using namespace mavlink_utils;
using namespace mavlinkcom_impl;

MavLinkConnectionImpl::MavLinkConnectionImpl()
{
    // add our custom telemetry message length.
    telemetry_.crc_errors = 0;
    telemetry_.handler_microseconds = 0;
    telemetry_.messages_handled = 0;
    telemetry_.messages_received = 0;
    telemetry_.messages_sent = 0;
    closed = true;
    ::memset(&mavlink_intermediate_status_, 0, sizeof(mavlink_status_t));
    ::memset(&mavlink_status_, 0, sizeof(mavlink_status_t));
    // todo: if we support signing then initialize
    // mavlink_intermediate_status_.signing callbacks
}

std::string MavLinkConnectionImpl::getName()
{
    return name;
}

MavLinkConnectionImpl::~MavLinkConnectionImpl()
{
    con_.reset();
    close();
}

std::shared_ptr<MavLinkConnection> MavLinkConnectionImpl::createConnection(const std::string& nodeName, std::shared_ptr<Port> port)
{
    // std::shared_ptr<MavLinkCom> owner, const std::string& nodeName
    std::shared_ptr<MavLinkConnection> con = std::make_shared<MavLinkConnection>();
    con->startListening(nodeName, port);
    return con;
}

std::shared_ptr<MavLinkConnection> MavLinkConnectionImpl::connectLocalUdp(const std::string& nodeName, const std::string& localAddr, int localPort)
{
    std::shared_ptr<UdpClientPort> socket = std::make_shared<UdpClientPort>();

    socket->connect(localAddr, localPort, "", 0);

    return createConnection(nodeName, socket);
}

std::shared_ptr<MavLinkConnection> MavLinkConnectionImpl::connectRemoteUdp(const std::string& nodeName, const std::string& localAddr, const std::string& remoteAddr, int remotePort)
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

std::shared_ptr<MavLinkConnection> MavLinkConnectionImpl::connectTcp(const std::string& nodeName, const std::string& localAddr, const std::string& remoteIpAddr, int remotePort)
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

std::string MavLinkConnectionImpl::acceptTcp(std::shared_ptr<MavLinkConnection> parent, const std::string& nodeName, const std::string& localAddr, int listeningPort)
{
    std::string local = localAddr;
    close();
    std::shared_ptr<TcpClientPort> socket = std::make_shared<TcpClientPort>();

    port = socket; // this is so that a call to close() can cancel this blocking accept call.
    socket->accept(localAddr, listeningPort);

    std::string remote = socket->remoteAddress();

    socket->setNonBlocking();
    socket->setNoDelay();

    parent->startListening(nodeName, socket);
    return remote;
}

std::shared_ptr<MavLinkConnection> MavLinkConnectionImpl::connectSerial(const std::string& nodeName, const std::string& portName, int baudRate, const std::string& initString)
{
    std::shared_ptr<SerialPort> serial = std::make_shared<SerialPort>();

    int hr = serial->connect(portName.c_str(), baudRate);
    if (hr != 0)
        throw std::runtime_error(Utils::stringf("Could not open the serial port %s, error=%d", portName.c_str(), hr));

    // send this right away just in case serial link is not already configured
    if (initString.size() > 0) {
        serial->write(reinterpret_cast<const uint8_t*>(initString.c_str()), static_cast<int>(initString.size()));
    }

    return createConnection(nodeName, serial);
}

void MavLinkConnectionImpl::startListening(std::shared_ptr<MavLinkConnection> parent, const std::string& nodeName, std::shared_ptr<Port> connectedPort)
{
    name = nodeName;
    con_ = parent;
    if (port != connectedPort) {
        close();
        port = connectedPort;
    }
    closed = false;

    Utils::cleanupThread(read_thread);
    read_thread = std::thread{ &MavLinkConnectionImpl::readPackets, this };
    Utils::cleanupThread(publish_thread_);
    publish_thread_ = std::thread{ &MavLinkConnectionImpl::publishPackets, this };
}

// log every message that is "sent" using sendMessage.
void MavLinkConnectionImpl::startLoggingSendMessage(std::shared_ptr<MavLinkLog> log)
{
    sendLog_ = log;
}

void MavLinkConnectionImpl::stopLoggingSendMessage()
{
    sendLog_ = nullptr;
}

// log every message that is "sent" using sendMessage.
void MavLinkConnectionImpl::startLoggingReceiveMessage(std::shared_ptr<MavLinkLog> log)
{
    receiveLog_ = log;
}

void MavLinkConnectionImpl::stopLoggingReceiveMessage()
{
    receiveLog_ = nullptr;
}

void MavLinkConnectionImpl::close()
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
    sendLog_ = nullptr;
    receiveLog_ = nullptr;
}

bool MavLinkConnectionImpl::isOpen()
{
    return !closed;
}

int MavLinkConnectionImpl::getTargetComponentId()
{
    return this->other_component_id;
}
int MavLinkConnectionImpl::getTargetSystemId()
{
    return this->other_system_id;
}

uint8_t MavLinkConnectionImpl::getNextSequence()
{
    std::lock_guard<std::mutex> guard(buffer_mutex);
    return next_seq++;
}

void MavLinkConnectionImpl::ignoreMessage(uint8_t message_id)
{
    ignored_messageids.insert(message_id);
}

void MavLinkConnectionImpl::sendMessage(const MavLinkMessage& m)
{
    if (ignored_messageids.find(m.msgid) != ignored_messageids.end())
        return;

    if (closed) {
        return;
    }

    {
        MavLinkMessage msg;
        ::memcpy(&msg, &m, sizeof(MavLinkMessage));
        prepareForSending(msg);

        if (sendLog_ != nullptr) {
            sendLog_->write(msg);
        }

        mavlink_message_t message;
        message.compid = msg.compid;
        message.sysid = msg.sysid;
        message.len = msg.len;
        message.checksum = msg.checksum;
        message.magic = msg.magic;
        message.incompat_flags = msg.incompat_flags;
        message.compat_flags = msg.compat_flags;
        message.seq = msg.seq;
        message.msgid = msg.msgid;
        ::memcpy(message.signature, msg.signature, 13);
        ::memcpy(message.payload64, msg.payload64, PayloadSize * sizeof(uint64_t));

        std::lock_guard<std::mutex> guard(buffer_mutex);
        unsigned len = mavlink_msg_to_send_buffer(message_buf, &message);

        try {
            port->write(message_buf, len);
        }
        catch (std::exception& e) {
            throw std::runtime_error(Utils::stringf("MavLinkConnectionImpl: Error sending message on connection '%s', details: %s", name.c_str(), e.what()));
        }
    }
    {
        std::lock_guard<std::mutex> guard(telemetry_mutex_);
        telemetry_.messages_sent++;
    }
}

int MavLinkConnectionImpl::prepareForSending(MavLinkMessage& msg)
{
    // as per  https://github.com/mavlink/mavlink/blob/master/doc/MAVLink2.md
    int seqno = getNextSequence();

    bool mavlink1 = !supports_mavlink2_ && msg.protocol_version != 2;
    bool signing = !mavlink1 && mavlink_status_.signing && (mavlink_status_.signing->flags & MAVLINK_SIGNING_FLAG_SIGN_OUTGOING);
    uint8_t signature_len = signing ? MAVLINK_SIGNATURE_BLOCK_LEN : 0;

    uint8_t header_len = MAVLINK_CORE_HEADER_LEN + 1;
    uint8_t buf[MAVLINK_CORE_HEADER_LEN + 1];
    if (mavlink1) {
        msg.magic = MAVLINK_STX_MAVLINK1;
        header_len = MAVLINK_CORE_HEADER_MAVLINK1_LEN + 1;
    }
    else {
        msg.magic = MAVLINK_STX;
    }

    msg.seq = seqno;
    msg.incompat_flags = 0;
    if (signing) {
        msg.incompat_flags |= MAVLINK_IFLAG_SIGNED;
    }
    msg.compat_flags = 0;

    // pack the payload buffer.
    char* payload = reinterpret_cast<char*>(&msg.payload64[0]);
    int len = msg.len;

    // calculate checksum
    const mavlink_msg_entry_t* entry = mavlink_get_msg_entry(msg.msgid);
    uint8_t crc_extra = 0;
    int msglen = 0;
    if (entry != nullptr) {
        crc_extra = entry->crc_extra;
        msglen = entry->min_msg_len;
    }
    if (msg.msgid == MavLinkTelemetry::kMessageId) {
        msglen = MavLinkTelemetry::MessageLength; // mavlink doesn't know about our custom telemetry message.
    }

    if (len != msglen) {
        if (mavlink1) {
            throw std::runtime_error(Utils::stringf("Message length %d doesn't match expected length%d\n", len, msglen));
        }
        else {
            // mavlink2 supports trimming the payload of trailing zeros so the messages
            // are variable length as a result.
        }
    }
    len = mavlink1 ? msglen : _mav_trim_payload(payload, msglen);
    msg.len = len;

    // form the header as a byte array for the crc
    buf[0] = msg.magic;
    buf[1] = msg.len;
    if (mavlink1) {
        buf[2] = msg.seq;
        buf[3] = msg.sysid;
        buf[4] = msg.compid;
        buf[5] = msg.msgid & 0xFF;
    }
    else {
        buf[2] = msg.incompat_flags;
        buf[3] = msg.compat_flags;
        buf[4] = msg.seq;
        buf[5] = msg.sysid;
        buf[6] = msg.compid;
        buf[7] = msg.msgid & 0xFF;
        buf[8] = (msg.msgid >> 8) & 0xFF;
        buf[9] = (msg.msgid >> 16) & 0xFF;
    }

    msg.checksum = crc_calculate(&buf[1], header_len - 1);
    crc_accumulate_buffer(&msg.checksum, payload, msg.len);
    crc_accumulate(crc_extra, &msg.checksum);

    // these macros use old style cast.
    STRICT_MODE_OFF
    mavlink_ck_a(&msg) = (uint8_t)(msg.checksum & 0xFF);
    mavlink_ck_b(&msg) = (uint8_t)(msg.checksum >> 8);
    STRICT_MODE_ON

    if (signing) {
        mavlink_sign_packet(mavlink_status_.signing,
                            reinterpret_cast<uint8_t*>(msg.signature),
                            reinterpret_cast<const uint8_t*>(message_buf),
                            header_len,
                            reinterpret_cast<const uint8_t*>(payload),
                            msg.len,
                            reinterpret_cast<const uint8_t*>(payload) + msg.len);
    }

    return msg.len + header_len + 2 + signature_len;
}

void MavLinkConnectionImpl::sendMessage(const MavLinkMessageBase& msg)
{
    MavLinkMessage m;
    msg.encode(m);
    sendMessage(m);
}

int MavLinkConnectionImpl::subscribe(MessageHandler handler)
{
    MessageHandlerEntry entry = { static_cast<int>(listeners.size() + 1), handler = handler };
    std::lock_guard<std::mutex> guard(listener_mutex);
    listeners.push_back(entry);
    snapshot_stale = true;
    return entry.id;
}

void MavLinkConnectionImpl::unsubscribe(int id)
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

void MavLinkConnectionImpl::joinLeftSubscriber(std::shared_ptr<MavLinkConnection> remote, std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg)
{
    unused(connection);
    // forward messages from our connected node to the remote proxy.
    if (supports_mavlink2_) {
        // tell the remote connection to expect mavlink2 messages.
        remote->pImpl->supports_mavlink2_ = true;
    }
    remote->sendMessage(msg);
}

void MavLinkConnectionImpl::joinRightSubscriber(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg)
{
    unused(connection);
    // forward messages from remote proxy to local connected node
    this->sendMessage(msg);
}

void MavLinkConnectionImpl::join(std::shared_ptr<MavLinkConnection> remote, bool subscribeToLeft, bool subscribeToRight)
{
    if (subscribeToLeft)
        this->subscribe(std::bind(&MavLinkConnectionImpl::joinLeftSubscriber, this, remote, std::placeholders::_1, std::placeholders::_2));

    if (subscribeToRight)
        remote->subscribe(std::bind(&MavLinkConnectionImpl::joinRightSubscriber, this, std::placeholders::_1, std::placeholders::_2));
}

void MavLinkConnectionImpl::readPackets()
{
    //CurrentThread::setMaximumPriority();
    CurrentThread::setThreadName("MavLinkThread");
    std::shared_ptr<Port> safePort = this->port;
    mavlink_message_t msg;
    mavlink_message_t msgBuffer; // intermediate state.
    const int MAXBUFFER = 512;
    uint8_t* buffer = new uint8_t[MAXBUFFER];
    mavlink_intermediate_status_.parse_state = MAVLINK_PARSE_STATE_IDLE;
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
        for (int i = 0; i < count; i++) {
            uint8_t frame_state = mavlink_frame_char_buffer(&msgBuffer, &mavlink_intermediate_status_, buffer[i], &msg, &mavlink_status_);

            if (frame_state == MAVLINK_FRAMING_INCOMPLETE) {
                continue;
            }
            else if (frame_state == MAVLINK_FRAMING_BAD_CRC) {
                std::lock_guard<std::mutex> guard(telemetry_mutex_);
                telemetry_.crc_errors++;
            }
            else if (frame_state == MAVLINK_FRAMING_OK) {
                // pick up the sysid/compid of the remote node we are connected to.
                if (other_system_id == -1) {
                    other_system_id = msg.sysid;
                    other_component_id = msg.compid;
                }

                if (mavlink_intermediate_status_.flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) {
                    // then this is a mavlink 1 message
                }
                else if (!supports_mavlink2_) {
                    // then this mavlink sender supports mavlink 2
                    supports_mavlink2_ = true;
                }

                if (con_ != nullptr && !closed) {
                    {
                        std::lock_guard<std::mutex> guard(telemetry_mutex_);
                        telemetry_.messages_received++;
                    }
                    // queue event for publishing.
                    {
                        std::lock_guard<std::mutex> guard(msg_queue_mutex_);
                        MavLinkMessage message;
                        message.compid = msg.compid;
                        message.sysid = msg.sysid;
                        message.len = msg.len;
                        message.checksum = msg.checksum;
                        message.magic = msg.magic;
                        message.incompat_flags = msg.incompat_flags;
                        message.compat_flags = msg.compat_flags;
                        message.seq = msg.seq;
                        message.msgid = msg.msgid;
                        message.protocol_version = supports_mavlink2_ ? 2 : 1;
                        ::memcpy(message.signature, msg.signature, 13);
                        ::memcpy(message.payload64, msg.payload64, PayloadSize * sizeof(uint64_t));
                        msg_queue_.push(message);
                    }
                    if (waiting_for_msg_) {
                        msg_available_.post();
                    }
                }
            }
            else {
                std::lock_guard<std::mutex> guard(telemetry_mutex_);
                telemetry_.crc_errors++;
            }
        }

    } //while

    delete[] buffer;

} //readPackets

void MavLinkConnectionImpl::drainQueue()
{
    MavLinkMessage message;
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

        if (receiveLog_ != nullptr) {
            receiveLog_->write(message);
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

        if (message.msgid == static_cast<uint8_t>(MavLinkMessageIds::MAVLINK_MSG_ID_AUTOPILOT_VERSION)) {
            MavLinkAutopilotVersion cap;
            cap.decode(message);
            if ((cap.capabilities & MAV_PROTOCOL_CAPABILITY_MAVLINK2) != 0) {
                this->supports_mavlink2_ = true;
            }
        }

        auto startTime = std::chrono::system_clock::now();
        std::shared_ptr<MavLinkConnection> sharedPtr = std::shared_ptr<MavLinkConnection>(this->con_);
        for (auto ptr = snapshot.begin(); ptr != end; ptr++) {
            try {
                (*ptr).handler(sharedPtr, message);
            }
            catch (std::exception& e) {
                Utils::log(Utils::stringf("MavLinkConnectionImpl: Error handling message %d on connection '%s', details: %s",
                                          message.msgid,
                                          name.c_str(),
                                          e.what()),
                           Utils::kLogLevelError);
            }
        }

        {
            auto endTime = std::chrono::system_clock::now();
            auto diff = endTime - startTime;
            long microseconds = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(diff).count());
            std::lock_guard<std::mutex> guard(telemetry_mutex_);
            telemetry_.messages_handled++;
            telemetry_.handler_microseconds += microseconds;
        }
    }
}

void MavLinkConnectionImpl::publishPackets()
{
    //CurrentThread::setMaximumPriority();
    CurrentThread::setThreadName("MavLinkThread");
    publish_thread_id_ = std::this_thread::get_id();
    while (!closed) {

        drainQueue();

        waiting_for_msg_ = true;
        msg_available_.wait();
        waiting_for_msg_ = false;
    }
}

bool MavLinkConnectionImpl::isPublishThread() const
{
    return std::this_thread::get_id() == publish_thread_id_;
}

void MavLinkConnectionImpl::getTelemetry(MavLinkTelemetry& result)
{
    std::lock_guard<std::mutex> guard(telemetry_mutex_);
    result = telemetry_;
    // reset counters
    telemetry_.crc_errors = 0;
    telemetry_.handler_microseconds = 0;
    telemetry_.messages_handled = 0;
    telemetry_.messages_received = 0;
    telemetry_.messages_sent = 0;
    if (telemetry_.wifiInterfaceName != nullptr) {
        telemetry_.wifi_rssi = port->getRssi(telemetry_.wifiInterfaceName);
    }
}
