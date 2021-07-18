// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkNodeImpl_hpp
#define MavLinkCom_MavLinkNodeImpl_hpp

#include "MavLinkNode.hpp"
#include "MavLinkConnection.hpp"

using namespace mavlinkcom;

namespace mavlinkcom_impl
{

class MavLinkNodeImpl
{
public:
    MavLinkNodeImpl(int localSystemId, int localComponentId);
    virtual ~MavLinkNodeImpl();

    void connect(std::shared_ptr<MavLinkConnection> connection);
    void close();

    // Send heartbeat to drone.  You should not do this if some other node is
    // already doing it.
    void startHeartbeat();

    std::vector<MavLinkParameter> getParamList();

    // get the parameter value cached from last getParamList call.
    MavLinkParameter getCachedParameter(const std::string& name);

    // get up to date value of this parametr
    AsyncResult<MavLinkParameter> getParameter(const std::string& name);

    AsyncResult<bool> setParameter(MavLinkParameter p);
    AsyncResult<MavLinkAutopilotVersion> getCapabilities();

    // get the connection
    std::shared_ptr<MavLinkConnection> getConnection()
    {
        return connection_;
    }

    std::shared_ptr<MavLinkConnection> ensureConnection()
    {
        if (connection_ == nullptr) {
            throw std::runtime_error("Cannot perform operation as there is no connection, did you forget to call connect() ?");
        }
        return connection_;
    }

    int getLocalSystemId()
    {
        return local_system_id;
    }
    int getLocalComponentId()
    {
        return local_component_id;
    }

    int getTargetSystemId()
    {
        return ensureConnection()->getTargetSystemId();
    }

    int getTargetComponentId()
    {
        return ensureConnection()->getTargetComponentId();
    }

    void setMessageInterval(int msgId, int frequency);

    AsyncResult<MavLinkHeartbeat> waitForHeartbeat();

    void sendOneHeartbeat();

    // Encode and send the given message to the connected node
    void sendMessage(MavLinkMessageBase& msg);

    // Send an already encoded messge to connected node
    void sendMessage(MavLinkMessage& msg);

    // send a command to the remote node
    void sendCommand(MavLinkCommand& cmd);

    // send a command to the remote node and return async result that tells you whether ACK was received or not.
    AsyncResult<bool> sendCommandAndWaitForAck(MavLinkCommand& cmd);

protected:
    // this is called for all messages received on the connection.
    virtual void handleMessage(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& message);
    void assertNotPublishingThread();

private:
    void sendHeartbeat();
    AsyncResult<MavLinkParameter> getParameterByIndex(int16_t index);
    bool inside_handle_message_;
    std::shared_ptr<MavLinkConnection> connection_;
    int subscription_ = 0;
    int local_system_id;
    int local_component_id;
    std::vector<MavLinkParameter> parameters_; //cached snapshot.
    MavLinkAutopilotVersion cap_;
    bool has_cap_ = false;
    bool req_cap_ = false;
    bool heartbeat_running_ = false;
    std::thread heartbeat_thread_;
};
}

#endif
