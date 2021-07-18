// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Utils.hpp"
#include "UdpClientPort.hpp"
#include <stdio.h>
#include <string.h>
#include "SocketInit.hpp"
#include "wifi.h"

using namespace mavlink_utils;

#ifdef _WIN32
// windows
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

typedef int socklen_t;
static bool socket_initialized_ = false;
inline int GetSocketError()
{
    return WSAGetLastError();
}
#else

// posix
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cerrno>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
typedef int SOCKET;
const int INVALID_SOCKET = -1;
const int ERROR_ACCESS_DENIED = EACCES;

inline int GetSocketError()
{
    return errno;
}
const int SOCKET_ERROR = -1;
#define E_NOT_SUFFICIENT_BUFFER ENOMEM

#endif

class UdpClientPort::UdpSocketImpl
{
    SocketInit init;
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in localaddr;
    sockaddr_in remoteaddr;
    bool hasRemote = false;
    bool closed_ = true;
    int retries_ = 0;
    const int max_retries_ = 10;

public:
    bool isClosed()
    {
        return closed_;
    }

    int getRssi(const char* ifaceName)
    {
        return getWifiRssi(static_cast<int>(sock), ifaceName);
    }

    static void resolveAddress(const std::string& ipAddress, int port, sockaddr_in& addr)
    {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        bool found = false;
        struct addrinfo* result = NULL;
        std::string service_name = std::to_string(port);
        int rc = getaddrinfo(ipAddress.c_str(), service_name.c_str(), &hints, &result);
        if (rc != 0) {
            auto msg = Utils::stringf("UdpClientPort getaddrinfo failed with error: %d\n", rc);
            throw std::runtime_error(msg);
        }
        for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            if (ptr->ai_family == AF_INET && ptr->ai_socktype == SOCK_DGRAM && ptr->ai_protocol == IPPROTO_UDP) {
                // found it!
                sockaddr_in* sptr = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
                addr.sin_family = sptr->sin_family;
                addr.sin_addr.s_addr = sptr->sin_addr.s_addr;
                addr.sin_port = sptr->sin_port;
                found = true;
                break;
            }
        }

        freeaddrinfo(result);
        if (!found) {
            auto msg = Utils::stringf("UdpClientPort could not resolve ip address for '%s:%d'\n", ipAddress.c_str(), port);
            throw std::runtime_error(msg);
        }
    }

    int connect(const std::string& localHost, int localPort, const std::string& remoteHost, int remotePort)
    {
        sock = socket(AF_INET, SOCK_DGRAM, 0);

        resolveAddress(localHost, localPort, localaddr);

        if (remoteHost != "") {
            hasRemote = true;
            resolveAddress(remoteHost, remotePort, remoteaddr);
        }
        else {
            remoteaddr.sin_port = 0;
        }

        // This timeout is important as it allows the MavLinkConnection readPackets
        // thread to iterate and notice the connection is now closed. This allows
        // AirSim to shutdown properly when drone is not connected.
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&tv), sizeof(tv));

        // bind socket to local address.
        socklen_t addrlen = sizeof(sockaddr_in);
        int rc = bind(sock, reinterpret_cast<sockaddr*>(&localaddr), addrlen);
        if (rc < 0) {
            int hr = GetSocketError();
            auto msg = Utils::stringf("UdpClientPort socket bind failed with error: %d\n", hr);
            throw std::runtime_error(msg);
            return hr;
        }

        if (hasRemote && remotePort != 0) {
            // limit the socket to only send/receive to/from this remote address/port, this ensures our
            // subsequent recvfrom calls don't steal messages from other UdpClientPorts.
            rc = ::connect(sock, reinterpret_cast<sockaddr*>(&remoteaddr), addrlen);
            if (rc < 0) {
                int hr = GetSocketError();
                auto msg = Utils::stringf("UdpClientPort socket could not connect to remote host at %s:%d, error: %d\n",
                                          remoteHost.c_str(),
                                          remotePort,
                                          hr);
                throw std::runtime_error(msg);
                return hr;
            }
        }
        closed_ = false;
        return 0;
    }

    int reconnect()
    {
        // try and reconnect
        std::string local_host = inet_ntoa(localaddr.sin_addr);
        std::string remote_host = remoteAddress();
        int local_port = 0;
        int remote_port = ntohs(remoteaddr.sin_port);
        int rc = connect(local_host, local_port, remote_host, remote_port);
        if (rc < 0) {
            GetSocketError();
        }
        return 0;
    }

    int checkerror()
    {
        int hr = GetSocketError();
        bool retry = false;
#ifdef _WIN32
        if (hr == WSAECONNRESET) {
            retry = true;
        }
#else
        if (hr == ECONNREFUSED || hr == ENOTCONN) {
            retry = true;
        }
#endif
        if (retry) {
            close();
            retries_++;
            if (retries_ < max_retries_) {
                return reconnect();
            }
        }
        return hr;
    }

    // write to the serial port
    int write(const uint8_t* ptr, int count)
    {
        if (closed_ || remoteaddr.sin_port == 0) {
            // well if we are creating a server, we don't know when the client is going to connect, so skip this exception for now.
            //throw std::runtime_error("UdpClientPort cannot send until we've received something first so we can find out what port to send to.\n");
            return 0;
        }

        socklen_t addrlen = sizeof(sockaddr_in);
#if defined(__APPLE__)
        int hr = ::write(sock, reinterpret_cast<const char*>(ptr), count);
#else
        int hr = sendto(sock, reinterpret_cast<const char*>(ptr), count, 0, reinterpret_cast<sockaddr*>(&remoteaddr), addrlen);
#endif
        if (hr == SOCKET_ERROR) {
            hr = checkerror();
            if (hr != 0) {
                // perhaps the client is gone, and may want to come back on a different port, in which case let's reset our remote port to allow that.
                remoteaddr.sin_port = 0;
                auto msg = Utils::stringf("UdpClientPort socket send failed with error: %d\n", hr);
                throw std::runtime_error(msg);
            }
        }

        return hr;
    }

    int read(uint8_t* result, int bytesToRead)
    {
        sockaddr_in other;

        int bytes_read = 0;
        // try and receive something, up until port is closed anyway.

        while (!closed_) {
            socklen_t addrlen = sizeof(sockaddr_in);
            int rc = recvfrom(sock, reinterpret_cast<char*>(result), bytesToRead, 0, reinterpret_cast<sockaddr*>(&other), &addrlen);
            if (rc < 0) {
                int hr = checkerror();
#ifdef _WIN32
                if (hr == WSAEMSGSIZE) {
                    // message was too large for the buffer, no problem, return what we have.
                }
                else if (hr == ERROR_IO_PENDING) {
                    // try again - this can happen if server recreates the socket on their side.
                    continue;
                }
                else if (hr == WSAEINTR) {
                    // skip this, it is was interrupted, and if user is closing the port closed_ will be true.
                    continue;
                }
                else if (hr == WSAETIMEDOUT) {
                    // skip this, the receive just timed out, no problem, we'll try again later.
                }
#else
                if (hr == EINTR) {
                    // skip this, it is was interrupted, and if user is closing the port closed_ will be true.
                    continue;
                }
                else
#endif
                {
                    //printf("#### recv failed with error: %d\n", hr);
                    return -1;
                }
            }

            if (remoteaddr.sin_port == 0) {
                // we now have it.
                remoteaddr.sin_family = other.sin_family;
                remoteaddr.sin_addr = other.sin_addr;
                remoteaddr.sin_port = other.sin_port;
            }
            else if (other.sin_addr.s_addr != remoteaddr.sin_addr.s_addr) {
                // this is from someone we are not interested in.
                continue;
            }

            if (rc == 0) {
                return -1;
            }
            else {
                return rc;
            }
        }
        return -1;
    }

    void close()
    {
        if (!closed_) {
            closed_ = true;

#ifdef _WIN32
            closesocket(sock);
#else
            int fd = static_cast<int>(sock);
            ::close(fd);
#endif
        }
    }

    std::string remoteAddress()
    {
        return inet_ntoa(remoteaddr.sin_addr);
    }

    int remotePort()
    {
        return ntohs(remoteaddr.sin_port);
    }
};

//-----------------------------------------------------------------------------------------

UdpClientPort::UdpClientPort()
{
    impl_.reset(new UdpSocketImpl());
}

UdpClientPort::~UdpClientPort()
{
    close();
}

void UdpClientPort::close()
{
    impl_->close();
}

void UdpClientPort::connect(const std::string& localHost, int localPort, const std::string& remoteHost, int remotePort)
{
    impl_->connect(localHost, localPort, remoteHost, remotePort);
}

int UdpClientPort::write(const uint8_t* ptr, int count)
{
    return impl_->write(ptr, count);
}

int UdpClientPort::read(uint8_t* buffer, int bytesToRead)
{
    return impl_->read(buffer, bytesToRead);
}

bool UdpClientPort::isClosed()
{
    return impl_->isClosed();
}

std::string UdpClientPort::remoteAddress()
{
    return impl_->remoteAddress();
}

int UdpClientPort::remotePort()
{
    return impl_->remotePort();
}

int UdpClientPort::getRssi(const char* ifaceName)
{
    return impl_->getRssi(ifaceName);
}