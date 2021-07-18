// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkNode.hpp"
#include "impl/MavLinkNodeImpl.hpp"
#include "AsyncResult.hpp"

using namespace mavlinkcom;
using namespace mavlinkcom_impl;

MavLinkNode::MavLinkNode(int localSystemId, int localComponentId)
    : pImpl{ new MavLinkNodeImpl(localSystemId, localComponentId) }
{
}

MavLinkNode::MavLinkNode()
    : pImpl(nullptr)
{
}

MavLinkNode::~MavLinkNode()
{
}

// start listening to this connection
void MavLinkNode::connect(std::shared_ptr<MavLinkConnection> connection)
{
    pImpl->connect(connection);
}

// Send heartbeat to drone.  You should not do this if some other node is
// already doing it.
void MavLinkNode::startHeartbeat()
{
    pImpl->startHeartbeat();
}

// stop listening to the connection.
void MavLinkNode::close()
{
    pImpl->close();
}

std::vector<MavLinkParameter> MavLinkNode::getParamList()
{
    return pImpl->getParamList();
}

MavLinkParameter MavLinkNode::getCachedParameter(const std::string& name)
{
    return pImpl->getCachedParameter(name);
}

AsyncResult<MavLinkParameter> MavLinkNode::getParameter(const std::string& name)
{
    return pImpl->getParameter(name);
}

AsyncResult<bool> MavLinkNode::setParameter(MavLinkParameter p)
{
    return pImpl->setParameter(p);
}

AsyncResult<MavLinkAutopilotVersion> MavLinkNode::getCapabilities()
{
    return pImpl->getCapabilities();
}

// get the connection
std::shared_ptr<MavLinkConnection> MavLinkNode::getConnection()
{
    return pImpl->getConnection();
}

AsyncResult<MavLinkHeartbeat> MavLinkNode::waitForHeartbeat()
{
    return pImpl->waitForHeartbeat();
}

void MavLinkNode::sendOneHeartbeat()
{
    return pImpl->sendOneHeartbeat();
}

void MavLinkNode::setMessageInterval(int msgId, int frequency)
{
    pImpl->setMessageInterval(msgId, frequency);
}

// Get the local system and component id
int MavLinkNode::getLocalSystemId()
{
    return pImpl->getLocalSystemId();
}
int MavLinkNode::getLocalComponentId()
{
    return pImpl->getLocalComponentId();
}
int MavLinkNode::getTargetSystemId()
{
    return pImpl->getTargetSystemId();
}
int MavLinkNode::getTargetComponentId()
{
    return pImpl->getTargetComponentId();
}

void MavLinkNode::sendMessage(MavLinkMessageBase& msg)
{
    pImpl->sendMessage(msg);
}

void MavLinkNode::sendMessage(MavLinkMessage& msg)
{
    pImpl->sendMessage(msg);
}

// send a command to the remote node
void MavLinkNode::sendCommand(MavLinkCommand& cmd)
{
    pImpl->sendCommand(cmd);
}

AsyncResult<bool> MavLinkNode::sendCommandAndWaitForAck(MavLinkCommand& cmd)
{
    return pImpl->sendCommandAndWaitForAck(cmd);
}

//MavLinkNode::MavLinkNode() = default;
//MavLinkNode::MavLinkNode(MavLinkNode&&) = default;
