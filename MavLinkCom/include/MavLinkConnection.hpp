// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef MavLinkCom_MavLinkConnection_hpp
#define MavLinkCom_MavLinkConnection_hpp

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "MavLinkMessageBase.hpp"
#include "MavLinkLog.hpp"

#ifndef ONECORE
#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Cfgmgr32.lib")
#endif
#endif

class Port;

namespace mavlinkcom_impl
{
class MavLinkConnectionImpl;
class MavLinkTcpServerImpl;
class MavLinkNodeImpl;
}

namespace mavlinkcom
{

class MavLinkConnection;
class MavLinkNode;

// This callback is invoked when a MavLink message is read from the connection.
typedef std::function<void(std::shared_ptr<MavLinkConnection> connection, const MavLinkMessage& msg)> MessageHandler;

// This callback is invoked when a new TCP connection is accepted via acceptTcp().
typedef std::function<void(std::shared_ptr<MavLinkConnection> port)> MavLinkConnectionHandler;

struct SerialPortInfo
{
    std::wstring displayName;
    std::wstring portName;
    int vid;
    int pid;
};

// This class represents a single connection to a remote mavlink node connected either over UDP, TCP or Serial port.
// You can use this connection in a MavLinkNode to send a message directly to that node, and start listening to messages
// from that remote node.  You can handle those messages directly using subscribe.
class MavLinkConnection : public std::enable_shared_from_this<MavLinkConnection>
{
public:
    MavLinkConnection();

    // Find available serial ports for the given vendor id/product id pair.  If a matching port is found it returns the
    // SerialPortInfo of that port, the portName can then be used connectSerial.  Pass 0 for vid and pid to find all
    // available serial ports.
    static std::vector<SerialPortInfo> findSerialPorts(int vid, int pid);

    // create connection over serial port (e.g. /dev/ttyACM0 or on windows "com5").
    // pass initial string to write to the port, which can be used to configure the port.
    // For example, on PX4 you can send "sh /etc/init.d/rc.usb\n" to turn on lots of mavlink streams.
    static std::shared_ptr<MavLinkConnection> connectSerial(const std::string& nodeName, const std::string& portName, int baudrate = 115200, const std::string& initString = "");

    // Start listening on a specific local port for packets from any remote computer.  Once a packet is received
    // it will remember the remote address of the sender so that subsequend sendMessage calls will go back to that sender.
    // This is useful if the remote sender already knows which local port you plan to listen on.
    // The localAddr can also a specific local ip address if you need to specify which
    // network interface to use, for example, a corporate wired ethernet usually does not transmit UDP packets
    // to a wifi connected device, so in that case the localAddress needs to be the IP address of a specific wifi internet
    // adapter rather than 127.0.0.1.
    static std::shared_ptr<MavLinkConnection> connectLocalUdp(const std::string& nodeName, const std::string& localAddr, int localPort);

    // Connect to a specific remote machine that is already listening on a specific port for messages from any computer.
    // This will use any free local port that is available.
    // The localAddr can also a specific local ip address if you need to specify which
    // network interface to use, for example, a corporate wired ethernet usually does not transmit UDP packets
    // to a wifi connected device, so in that case the localAddress needs to be the IP address of a specific wifi internet
    // adapter rather than 127.0.0.1.
    static std::shared_ptr<MavLinkConnection> connectRemoteUdp(const std::string& nodeName, const std::string& localAddr, const std::string& remoteAddr, int remotePort);

    // This method sets up a tcp connection to the specified remote host and port.  The remote host
    // must already be listening and accepting TCP socket connections for this to succeed.
    // The  localAddr can also a specific local ip address if you need to specify which
    // NIC to use, for example, wifi versus hard wired ethernet adapter.  For localhost pass 127.0.0.1.
    static std::shared_ptr<MavLinkConnection> connectTcp(const std::string& nodeName, const std::string& localAddr, const std::string& remoteIpAddr, int remotePort);

    // This method accepts one tcp connection from a remote host on a given port.
    // You may need to open this port in your firewall.
    // The  localAddr can also a specific local ip address if you need to specify which
    // NIC to use, for example, wifi versus hard wired ethernet adapter.  For localhost pass 127.0.0.1.
    // It returns the address of the remote machine that connected.
    std::string acceptTcp(const std::string& nodeName, const std::string& localAddr, int listeningPort);

    // instance methods
    std::string getName();
    int getTargetComponentId();
    int getTargetSystemId();
    bool isOpen();
    void close();

    // provide a callback function that will be called for every message "received" from the remote mavlink node.
    int subscribe(MessageHandler handler);
    void unsubscribe(int id);

    // log every message that is "sent" using sendMessage.
    void startLoggingSendMessage(std::shared_ptr<MavLinkLog> log);
    void stopLoggingSendMessage();

    // log every message that is "received" by a subscriber.
    void startLoggingReceiveMessage(std::shared_ptr<MavLinkLog> log);
    void stopLoggingReceiveMessage();

    uint8_t getNextSequence();

    // Advanced method that create a bridge between two connections.  For example, if you use connectRemoteUdp to connect to
    // QGroundControl port 14550, and connectSerial to connect to PX4, then you can call this method to join the two so that
    // all messages from PX4 are sent to QGroundControl and vice versa.
    void join(std::shared_ptr<MavLinkConnection> remote, bool subscribeToLeft = true, bool subscribeToRight = true);

    // Pack and send the given message, assuming the compid and sysid have been set by the caller.
    void sendMessage(const MavLinkMessageBase& msg);

    // Send the given already encoded message, assuming the compid and sysid have been set by the caller.
    void sendMessage(const MavLinkMessage& msg);

    // get the next telemetry snapshot, then clear the internal counters and start over.  This way each snapshot
    // gives you a picture of what happened in whatever timeslice you decide to call this method.  This is packaged
    // in a mavlink message so you can easily send it to the LogViewer.
    void getTelemetry(MavLinkTelemetry& result);

    //add the message in to list of ignored messages. These messages will not be sent in the sendMessage() call.
    //this does not effect reception of message, however. This is typically useful in scenario where many connections
    //are bridged and you don't want certain connection to read ceratin messages.
    void ignoreMessage(uint8_t message_id);

    // Compute crc checksums, and pack according to mavlink1 or mavlink2 (depending on what target node supports) and do optional
    // message signing according to the target node we are communicating with, and return the message length.
    int prepareForSending(MavLinkMessage& msg);

    // Returns true if we are on the publishing thread.  Certain blocing operations that wait for messages from mavlin vehicle are not
    // allowed on this thread.
    bool isPublishThread() const;

protected:
    void startListening(const std::string& nodeName, std::shared_ptr<Port> connectedPort);

public:
    //needed for piml pattern
    ~MavLinkConnection();
    //MavLinkConnection(MavLinkConnection&&);
    //MavLinkConnection& operator=(MavLinkConnection&&);
private:
    std::unique_ptr<mavlinkcom_impl::MavLinkConnectionImpl> pImpl;
    friend class MavLinkNode;
    friend class mavlinkcom_impl::MavLinkNodeImpl;
    friend class mavlinkcom_impl::MavLinkConnectionImpl;
    friend class mavlinkcom_impl::MavLinkTcpServerImpl;
};
}

#endif
