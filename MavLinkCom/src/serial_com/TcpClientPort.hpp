// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERIAL_COM_TCPCLIENTPORT_HPP
#define SERIAL_COM_TCPCLIENTPORT_HPP

#include "Port.h"

class TcpClientPort : public Port
{
public:
    TcpClientPort();
    virtual ~TcpClientPort();

    // Connect can set you up two different ways.  Pass 0 for local port to get any free local
    // port. localHost allows you to be specific about which local adapter to use in case you
    // have multiple ethernet adapters.
    void connect(const std::string& localHost, int localPort, const std::string& remoteHost, int remotePort);

    // start listening on the local adapter, and accept one connection request from a remote machine.
    void accept(const std::string& localHost, int localPort);

    // write the given bytes to the port, return number of bytes written or -1 if error.
    int write(const uint8_t* ptr, int count);

    // read some bytes from the port, return the number of bytes read or -1 if error.
    int read(uint8_t* buffer, int bytesToRead);

    // close the port.
    void close();

    bool isClosed();
    int getRssi(const char* ifaceName);
    std::string remoteAddress();
    int remotePort();

    void setNonBlocking();
    void setNoDelay();

private:
    class TcpSocketImpl;
    std::unique_ptr<TcpSocketImpl> impl_;
};

#endif // SERIAL_COM_UDPCLIENTPORT_HPP
