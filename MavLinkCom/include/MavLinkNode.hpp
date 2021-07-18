// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkNode_hpp
#define MavLinkCom_MavLinkNode_hpp

#include <memory>
#include <vector>
#include "AsyncResult.hpp"
#include "MavLinkConnection.hpp"
#include "MavLinkMessages.hpp"

namespace mavlinkcom_impl
{
class MavLinkNodeImpl;
}
namespace mavlinkcom
{

struct MavLinkParameter
{
public:
    std::string name;
    int index = -1;
    float value = 0;
    uint8_t type = 0;
};

class MavLinkCommand;

// This is the base class for all MavLink objects that represent a single
// mavlink node.  This class provides high level state that applies to any
// kind of mavlink node whether it is a vehicle, ground control, simulator
// or log viewer.   This base class also implements the heartbeat protocol
// which pings heart beats back to the remote node to ensure it knows our
// connection is still alive.
class MavLinkNode
{
public:
    MavLinkNode(int localSystemId, int localComponentId);
    ~MavLinkNode();

    // start listening to this connection
    void connect(std::shared_ptr<MavLinkConnection> connection);

    // stop listening to the connection.
    void close();

    // Send heartbeat to drone.  You should not do this if some other node is
    // already doing it.
    void startHeartbeat();

    // get the list of configurable parameters supported by this node.
    std::vector<MavLinkParameter> getParamList();

    // get the parameter from last getParamList download.
    MavLinkParameter getCachedParameter(const std::string& name);

    // get a single parameter by name.
    AsyncResult<MavLinkParameter> getParameter(const std::string& name);

    // set a new value on a given parameter.
    // it is best if you use getParameter to get the current value, see if it
    // needs changing, change the value, then call setParameter with the same parameter object.
    AsyncResult<bool> setParameter(MavLinkParameter p);

    // get the connection
    std::shared_ptr<MavLinkConnection> getConnection();

    // get the capabilities of the drone
    AsyncResult<MavLinkAutopilotVersion> getCapabilities();

    // request that target node sends this message stream at given frequency (specify messages per second that you want).
    // where msgId is a MAVLINK_MSG_ID_* enum value.
    void setMessageInterval(int msgId, int frequency);

    // wait a given amount of time for a heart beat, and return the decoded message.
    AsyncResult<MavLinkHeartbeat> waitForHeartbeat();

    // send a single heartbeat
    void sendOneHeartbeat();

    // Get the local system and component id
    int getLocalSystemId();
    int getLocalComponentId();

    // Get the target system and component id on the other end of the connection.
    int getTargetSystemId();
    int getTargetComponentId();

    // Send a message to the remote node
    void sendMessage(MavLinkMessageBase& msg);

    // Send raw undecoded messge to remote node (this is handy in proxy scenarios where you are simply
    // passing a message along).
    void sendMessage(MavLinkMessage& msg);

    // send a command to the remote node
    void sendCommand(MavLinkCommand& cmd);

    // send a command to the remote node and get async result that tells you whether it succeeded or not.
    AsyncResult<bool> sendCommandAndWaitForAck(MavLinkCommand& cmd);

public:
    MavLinkNode();
    //MavLinkNode(MavLinkNode&&);
protected:
    std::unique_ptr<mavlinkcom_impl::MavLinkNodeImpl> pImpl;
};
}

#endif
