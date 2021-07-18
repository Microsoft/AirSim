// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "MavLinkFtpClient.hpp"
#include "impl/MavLinkFtpClientImpl.hpp"

using namespace mavlinkcom;
using namespace mavlinkcom_impl;

MavLinkFtpClient::MavLinkFtpClient(int localSystemId, int localComponentId)
    : MavLinkNode(localSystemId, localComponentId)
{
    pImpl.reset(new MavLinkFtpClientImpl(localSystemId, localComponentId));
}

MavLinkFtpClient::~MavLinkFtpClient()
{
}

bool MavLinkFtpClient::isSupported()
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    return ptr->isSupported();
}
void MavLinkFtpClient::cancel()
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    ptr->cancel();
}

void MavLinkFtpClient::list(MavLinkFtpProgress& progress, const std::string& remotePath, std::vector<MavLinkFileInfo>& files)
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    return ptr->list(progress, remotePath, files);
}

void MavLinkFtpClient::get(MavLinkFtpProgress& progress, const std::string& remotePath, const std::string& localPath)
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    ptr->get(progress, remotePath, localPath);
}

void MavLinkFtpClient::put(MavLinkFtpProgress& progress, const std::string& remotePath, const std::string& localPath)
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    ptr->put(progress, remotePath, localPath);
}

void MavLinkFtpClient::remove(MavLinkFtpProgress& progress, const std::string& remotePath)
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    ptr->remove(progress, remotePath);
}

void MavLinkFtpClient::mkdir(MavLinkFtpProgress& progress, const std::string& remotePath)
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    ptr->mkdir(progress, remotePath);
}

void MavLinkFtpClient::rmdir(MavLinkFtpProgress& progress, const std::string& remotePath)
{
    auto ptr = dynamic_cast<MavLinkFtpClientImpl*>(pImpl.get());
    ptr->rmdir(progress, remotePath);
}