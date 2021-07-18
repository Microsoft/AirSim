#include "AdHocConnection.hpp"
#include "impl/AdHocConnectionImpl.hpp"

using namespace mavlinkcom;
using namespace mavlinkcom_impl;

AdHocConnection::AdHocConnection()
{
    pImpl.reset(new AdHocConnectionImpl());
}

std::shared_ptr<AdHocConnection> AdHocConnection::connectLocalUdp(const std::string& nodeName, std::string localAddr, int localPort)
{
    return AdHocConnectionImpl::connectLocalUdp(nodeName, localAddr, localPort);
}

std::shared_ptr<AdHocConnection> AdHocConnection::connectRemoteUdp(const std::string& nodeName, std::string localAddr, std::string remoteAddr, int remotePort)
{
    return AdHocConnectionImpl::connectRemoteUdp(nodeName, localAddr, remoteAddr, remotePort);
}

void AdHocConnection::startListening(const std::string& nodeName, std::shared_ptr<Port> connectedPort)
{
    pImpl->startListening(shared_from_this(), nodeName, connectedPort);
}

void AdHocConnection::close()
{
    pImpl->close();
}

bool AdHocConnection::isOpen()
{
    return pImpl->isOpen();
}

void AdHocConnection::sendMessage(const std::vector<uint8_t>& msg)
{
    pImpl->sendMessage(msg);
}

int AdHocConnection::subscribe(AdHocMessageHandler handler)
{
    return pImpl->subscribe(handler);
}

void AdHocConnection::unsubscribe(int id)
{
    pImpl->unsubscribe(id);
}

AdHocConnection::~AdHocConnection()
{
    pImpl->close();
    pImpl = nullptr;
}
