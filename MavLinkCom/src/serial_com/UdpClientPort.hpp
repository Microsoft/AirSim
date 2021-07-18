// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERIAL_COM_UDPCLIENTPORT_HPP
#define SERIAL_COM_UDPCLIENTPORT_HPP

#include "Port.h"

class UdpClientPort : public Port
{
public:
    UdpClientPort();
    virtual ~UdpClientPort();

    // Connect can set you up two different ways.  Pass 0 for local port to get any free local
    // port and pass a fixed remotePort if you want to send to a specific remote port.
    // Conversely, pass a fix local port to bind to, and 0 for remotePort if you want to
    // allow any remote sender to send to your specific local port.  localHost allows you to
    // be specific about local adapter ip address.
    void connect(const std::string& localHost, int localPort, const std::string& remoteHost, int remotePort);

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

private:
    void reconnect();
    class UdpSocketImpl;
    std::unique_ptr<UdpSocketImpl> impl_;
};

#endif // SERIAL_COM_UDPCLIENTPORT_HPP
