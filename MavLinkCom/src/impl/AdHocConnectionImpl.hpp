// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_AdHocConnectionImpl_hpp
#define MavLinkCom_AdHocConnectionImpl_hpp

#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <unordered_set>
#include "AdHocConnection.hpp"
//#include "MavLinkMessageBase.hpp"
#include "Semaphore.hpp"
#include "../serial_com/TcpClientPort.hpp"
#include "StrictMode.hpp"
#define MAVLINK_PACKED

STRICT_MODE_OFF
#include "../mavlink/common/mavlink.h"
#include "../mavlink/mavlink_helpers.h"
#include "../mavlink/mavlink_types.h"
STRICT_MODE_ON

using namespace mavlinkcom;

namespace mavlinkcom_impl
{

// See MavLinkConnection.hpp for definitions of these methods.
class AdHocConnectionImpl
{
public:
    AdHocConnectionImpl();
    static std::shared_ptr<AdHocConnection> connectSerial(const std::string& nodeName, std::string portName, int baudrate = 115200, const std::string initString = "");
    static std::shared_ptr<AdHocConnection> connectLocalUdp(const std::string& nodeName, std::string localAddr, int localPort);
    static std::shared_ptr<AdHocConnection> connectRemoteUdp(const std::string& nodeName, std::string localAddr, std::string remoteAddr, int remotePort);
    static std::shared_ptr<AdHocConnection> connectTcp(const std::string& nodeName, std::string localAddr, const std::string& remoteIpAddr, int remotePort);

    std::string getName();
    int getTargetComponentId();
    int getTargetSystemId();
    ~AdHocConnectionImpl();
    void startListening(std::shared_ptr<AdHocConnection> parent, const std::string& nodeName, std::shared_ptr<Port> connectedPort);
    void close();
    bool isOpen();
    void sendMessage(const std::vector<uint8_t>& msg);
    int subscribe(AdHocMessageHandler handler);
    void unsubscribe(int id);

private:
    static std::shared_ptr<AdHocConnection> createConnection(const std::string& nodeName, std::shared_ptr<Port> port);
    void publishPackets();
    void readPackets();
    void drainQueue();
    std::string name;
    std::shared_ptr<Port> port;
    std::shared_ptr<AdHocConnection> con_;
    int other_system_id = -1;
    int other_component_id = 0;
    std::thread read_thread;
    std::string accept_node_name_;
    std::shared_ptr<TcpClientPort> server_;

    struct MessageHandlerEntry
    {
    public:
        int id;
        AdHocMessageHandler handler;
    };
    std::vector<MessageHandlerEntry> listeners;
    std::vector<MessageHandlerEntry> snapshot;
    bool snapshot_stale;
    std::mutex listener_mutex;
    bool closed;
    std::thread publish_thread_;
    std::queue<std::vector<uint8_t>> msg_queue_;
    std::mutex msg_queue_mutex_;
    mavlink_utils::Semaphore msg_available_;
    bool waiting_for_msg_ = false;
    bool supports_mavlink2_ = false;
    bool signing_ = false;
    mavlink_status_t mavlink_intermediate_status_;
    mavlink_status_t mavlink_status_;
    std::mutex telemetry_mutex_;
    std::unordered_set<uint8_t> ignored_messageids;
};
}

#endif
