// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkConnectionImpl_hpp
#define MavLinkCom_MavLinkConnectionImpl_hpp

#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <unordered_set>
#include "MavLinkConnection.hpp"
#include "MavLinkMessageBase.hpp"
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
class MavLinkConnectionImpl
{
public:
    MavLinkConnectionImpl();
    static std::shared_ptr<MavLinkConnection> connectSerial(const std::string& nodeName, const std::string& portName, int baudRate = 115200, const std::string& initString = "");
    static std::shared_ptr<MavLinkConnection> connectLocalUdp(const std::string& nodeName, const std::string& localAddr, int localPort);
    static std::shared_ptr<MavLinkConnection> connectRemoteUdp(const std::string& nodeName, const std::string& localAddr, const std::string& remoteAddr, int remotePort);
    static std::shared_ptr<MavLinkConnection> connectTcp(const std::string& nodeName, const std::string& localAddr, const std::string& remoteIpAddr, int remotePort);
    std::string acceptTcp(std::shared_ptr<MavLinkConnection> parent, const std::string& nodeName, const std::string& localAddr, int listeningPort);

    std::string getName();
    int getTargetComponentId();
    int getTargetSystemId();
    ~MavLinkConnectionImpl();
    void startListening(std::shared_ptr<MavLinkConnection> parent, const std::string& nodeName, std::shared_ptr<Port> connectedPort);
    void startLoggingSendMessage(std::shared_ptr<MavLinkLog> log);
    void stopLoggingSendMessage();
    void startLoggingReceiveMessage(std::shared_ptr<MavLinkLog> log);
    void stopLoggingReceiveMessage();
    void close();
    bool isOpen();
    void sendMessage(const MavLinkMessageBase& msg);
    void sendMessage(const MavLinkMessage& msg);
    int subscribe(MessageHandler handler);
    void unsubscribe(int id);
    uint8_t getNextSequence();
    void join(std::shared_ptr<MavLinkConnection> remote, bool subscribeToLeft = true, bool subscribeToRight = true);
    void getTelemetry(MavLinkTelemetry& result);
    void ignoreMessage(uint8_t message_id);
    int prepareForSending(MavLinkMessage& msg);
    bool isPublishThread() const;

private:
    static std::shared_ptr<MavLinkConnection> createConnection(const std::string& nodeName, std::shared_ptr<Port> port);
    void joinLeftSubscriber(std::shared_ptr<MavLinkConnection> remote, std::shared_ptr<MavLinkConnection> con, const MavLinkMessage& msg);
    void joinRightSubscriber(std::shared_ptr<MavLinkConnection> con, const MavLinkMessage& msg);
    void publishPackets();
    void readPackets();
    void drainQueue();
    std::string name;
    std::shared_ptr<Port> port;
    std::shared_ptr<MavLinkConnection> con_;
    int other_system_id = -1;
    int other_component_id = 0;
    uint8_t next_seq = 0;
    std::thread read_thread;
    std::string accept_node_name_;
    std::shared_ptr<TcpClientPort> server_;
    std::shared_ptr<MavLinkLog> sendLog_;
    std::shared_ptr<MavLinkLog> receiveLog_;

    struct MessageHandlerEntry
    {
    public:
        int id;
        MessageHandler handler;
    };
    std::vector<MessageHandlerEntry> listeners;
    std::vector<MessageHandlerEntry> snapshot;
    bool snapshot_stale;
    std::mutex listener_mutex;
    uint8_t message_buf[300]; // must be bigger than sizeof(mavlink_message_t), which is currently 292.
    std::mutex buffer_mutex;
    bool closed;
    std::thread publish_thread_;
    std::queue<MavLinkMessage> msg_queue_;
    std::mutex msg_queue_mutex_;
    mavlink_utils::Semaphore msg_available_;
    bool waiting_for_msg_ = false;
    bool supports_mavlink2_ = false;
    std::thread::id publish_thread_id_;
    mavlink_status_t mavlink_intermediate_status_;
    mavlink_status_t mavlink_status_;
    std::mutex telemetry_mutex_;
    MavLinkTelemetry telemetry_;
    std::unordered_set<uint8_t> ignored_messageids;
};
}

#endif
