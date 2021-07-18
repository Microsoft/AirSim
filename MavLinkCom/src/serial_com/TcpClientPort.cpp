// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "Utils.hpp"
#include "TcpClientPort.hpp"
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
#include <netinet/tcp.h>
#include <cerrno>
#include <fcntl.h>
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

class TcpClientPort::TcpSocketImpl
{
    SocketInit init;
    SOCKET sock = INVALID_SOCKET;
    SOCKET accept_sock = INVALID_SOCKET;
    sockaddr_in localaddr;
    sockaddr_in remoteaddr;
    bool closed_ = true;

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
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        bool found = false;
        struct addrinfo* result = NULL;
        std::string serviceName = std::to_string(port);
        int rc = getaddrinfo(ipAddress.c_str(), serviceName.c_str(), &hints, &result);
        if (rc != 0) {
            auto msg = Utils::stringf("TcpClientPort getaddrinfo failed with error: %d\n", rc);
            throw std::runtime_error(msg);
        }
        for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            if (ptr->ai_family == AF_INET && ptr->ai_socktype == SOCK_STREAM && ptr->ai_protocol == IPPROTO_TCP) {
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
            auto msg = Utils::stringf("TcpClientPort could not resolve ip address for '%s:%d'\n", ipAddress.c_str(), port);
            throw std::runtime_error(msg);
        }
    }

    int connect(const std::string& localHost, int localPort, const std::string& remoteHost, int remotePort)
    {
        sock = socket(AF_INET, SOCK_STREAM, 0);

        resolveAddress(localHost, localPort, localaddr);
        resolveAddress(remoteHost, remotePort, remoteaddr);

        // bind socket to local address.
        socklen_t addrlen = sizeof(sockaddr_in);
        int rc = bind(sock, reinterpret_cast<sockaddr*>(&localaddr), addrlen);
        if (rc < 0) {
            int hr = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort socket bind failed with error: %d\n", hr);
            throw std::runtime_error(msg);
        }

        rc = ::connect(sock, reinterpret_cast<sockaddr*>(&remoteaddr), addrlen);
        if (rc != 0) {
            int hr = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort socket connect failed with error: %d\n", hr);
            throw std::runtime_error(msg);
        }

        closed_ = false;
        return 0;
    }

    void accept(const std::string& localHost, int localPort)
    {
        accept_sock = socket(AF_INET, SOCK_STREAM, 0);

        resolveAddress(localHost, localPort, localaddr);

        // bind socket to local address.
        socklen_t addrlen = sizeof(sockaddr_in);
        int rc = ::bind(accept_sock, reinterpret_cast<sockaddr*>(&localaddr), addrlen);
        if (rc < 0) {
            int hr = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort socket bind failed with error: %d\n", hr);
            throw std::runtime_error(msg);
        }

        // start listening for incoming connection
        rc = ::listen(accept_sock, 1);
        if (rc < 0) {
            int hr = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort socket listen failed with error: %d\n", hr);
            throw std::runtime_error(msg);
        }

        // accept 1
        sock = ::accept(accept_sock, reinterpret_cast<sockaddr*>(&remoteaddr), &addrlen);
        if (sock == INVALID_SOCKET) {
            int hr = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort accept failed with error: %d\n", hr);
            throw std::runtime_error(msg);
        }

#ifdef _WIN32
        // don't need to accept any more, so we can close this one.
        ::closesocket(accept_sock);
#else
        int fd = static_cast<int>(accept_sock);
        ::close(fd);
#endif
        accept_sock = INVALID_SOCKET;

        closed_ = false;
    }

    void setNonBlocking()
    {
#ifdef _WIN32
        unsigned long mode = 1;
        int rc = ioctlsocket(sock, FIONBIO, &mode);
#else
        int fd = static_cast<int>(sock);
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            auto msg = Utils::stringf("fcntl failed with error: %d\n", errno);
            throw std::runtime_error(msg);
        }
        flags |= O_NONBLOCK;
        int rc = fcntl(fd, F_SETFL, flags);
#endif
        if (rc != 0) {
            rc = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort setNonBlocking failed with error: %d\n", rc);
            throw std::runtime_error(msg);
        }
    }

    void setNoDelay()
    {
        int flags = 1;
#ifdef _WIN32
        int rc = ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flags), sizeof(flags));
#else
        int fd = static_cast<int>(sock);
        int rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flags), sizeof(flags));
#endif

        if (rc != 0) {
            rc = GetSocketError();
            auto msg = Utils::stringf("TcpClientPort set TCP_NODELAY failed: %d\n", rc);
            throw std::runtime_error(msg);
        }
    }

    // write to the serial port
    int write(const uint8_t* ptr, int count)
    {
        int hr = send(sock, reinterpret_cast<const char*>(ptr), count, 0);
        if (hr == SOCKET_ERROR) {
            hr = checkerror();
            auto msg = Utils::stringf("TcpClientPort socket send failed with error: %d\n", hr);
            throw std::runtime_error(msg);
        }

        return hr;
    }

    int checkerror()
    {
        int hr = GetSocketError();
#ifdef _WIN32
        if (hr == WSAECONNRESET) {
            close();
        }
#else
        if (hr == ECONNREFUSED || hr == ENOTCONN) {
            close();
        }
#endif
        return hr;
    }

    int read(uint8_t* result, int bytesToRead)
    {
        int bytesRead = 0;
        // try and receive something, up until port is closed anyway.

        while (!closed_) {
            int rc = recv(sock, reinterpret_cast<char*>(result), bytesToRead, 0);
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
                else
#else
                if (hr == EINTR) {
                    // try again - this can happen if server recreates the socket on their side.
                    continue;
                }
                else
#endif
                {
                    return -1;
                }
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
        closed_ = true;
        if (sock != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(sock);
#else
            int fd = static_cast<int>(sock);
            ::close(fd);
#endif
            sock = INVALID_SOCKET;
        }

        if (accept_sock != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(accept_sock);
#else
            int fd = static_cast<int>(accept_sock);
            ::close(fd);
#endif
            accept_sock = INVALID_SOCKET;
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

TcpClientPort::TcpClientPort()
{
    impl_.reset(new TcpSocketImpl());
}

TcpClientPort::~TcpClientPort()
{
    close();
}

void TcpClientPort::close()
{
    impl_->close();
}

void TcpClientPort::connect(const std::string& localHost, int localPort, const std::string& remoteHost, int remotePort)
{
    impl_->connect(localHost, localPort, remoteHost, remotePort);
}

void TcpClientPort::accept(const std::string& localHost, int localPort)
{
    impl_->accept(localHost, localPort);
}

int TcpClientPort::write(const uint8_t* ptr, int count)
{
    return impl_->write(ptr, count);
}

int TcpClientPort::read(uint8_t* buffer, int bytesToRead)
{
    return impl_->read(buffer, bytesToRead);
}

bool TcpClientPort::isClosed()
{
    return impl_->isClosed();
}

std::string TcpClientPort::remoteAddress()
{
    return impl_->remoteAddress();
}

int TcpClientPort::remotePort()
{
    return impl_->remotePort();
}

int TcpClientPort::getRssi(const char* ifaceName)
{
    return impl_->getRssi(ifaceName);
}

void TcpClientPort::setNoDelay()
{
    impl_->setNoDelay();
}

void TcpClientPort::setNonBlocking()
{
    impl_->setNonBlocking();
}
