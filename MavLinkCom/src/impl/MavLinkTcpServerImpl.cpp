// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkTcpServerImpl.hpp"
#include "../serial_com/TcpClientPort.hpp"

using namespace mavlinkcom_impl;

MavLinkTcpServerImpl::MavLinkTcpServerImpl(const std::string& local_addr, int local_port)
{
    local_address_ = local_addr;
    local_port_ = local_port;
}

MavLinkTcpServerImpl::~MavLinkTcpServerImpl()
{
}

std::shared_ptr<MavLinkConnection> MavLinkTcpServerImpl::acceptTcp(const std::string& nodeName)
{
    accept_node_name_ = nodeName;

    std::shared_ptr<TcpClientPort> result = std::make_shared<TcpClientPort>();
    result->accept(local_address_, local_port_);

    auto con = std::make_shared<MavLinkConnection>();
    con->startListening(nodeName, result);
    return con;
}
