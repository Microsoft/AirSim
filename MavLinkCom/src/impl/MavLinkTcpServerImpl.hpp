// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkTcpServerImpl_hpp
#define MavLinkCom_MavLinkTcpServerImpl_hpp

#include <memory>
#include <vector>
#include <string>
#include "MavLinkTcpServer.hpp"

using namespace mavlinkcom;

class TcpClientPort;

namespace mavlinkcom_impl
{
class MavLinkTcpServerImpl
{
public:
    MavLinkTcpServerImpl(const std::string& local_addr, int local_port);
    ~MavLinkTcpServerImpl();

    // accept one new connection from a remote machine.
    std::shared_ptr<MavLinkConnection> acceptTcp(const std::string& nodeName);

private:
    std::string local_address_;
    int local_port_;
    std::string accept_node_name_;
    std::shared_ptr<TcpClientPort> server_;
};
}

#endif
