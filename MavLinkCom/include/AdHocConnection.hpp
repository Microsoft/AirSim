#ifndef MavLinkCom_AdHocConnection_hpp
#define MavLinkCom_AdHocConnection_hpp

#include <functional>
#include <memory>
#include <string>
#include <vector>

#ifndef ONECORE
#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Cfgmgr32.lib")
#endif
#endif

class Port;

namespace mavlinkcom_impl
{
class AdHocConnectionImpl;
}

namespace mavlinkcom
{

class AdHocConnection;

// This callback is invoked when a MavLink message is read from the connection.
typedef std::function<void(std::shared_ptr<AdHocConnection> connection, const std::vector<uint8_t>& msg)> AdHocMessageHandler;

// This class represents a single connection to a remote non-mavlink node connected either over UDP, TCP or Serial port.
// You can use this connection to send a message directly to that node, and start listening to messages
// from that remote node.  You can handle those messages directly using subscribe.
class AdHocConnection : public std::enable_shared_from_this<AdHocConnection>
{
public:
    AdHocConnection();

    // Start listening on a specific local port for packets from any remote computer.  Once a packet is received
    // it will remember the remote address of the sender so that subsequend sendMessage calls will go back to that sender.
    // This is useful if the remote sender already knows which local port you plan to listen on.
    // The localAddr can also a specific local ip address if you need to specify which
    // network interface to use, for example, a corporate wired ethernet usually does not transmit UDP packets
    // to a wifi connected device, so in that case the localAddress needs to be the IP address of a specific wifi internet
    // adapter rather than 127.0.0.1.
    static std::shared_ptr<AdHocConnection> connectLocalUdp(const std::string& nodeName, std::string localAddr, int localPort);

    // Connect to a specific remote machine that is already listening on a specific port for messages from any computer.
    // This will use any free local port that is available.
    // The localAddr can also a specific local ip address if you need to specify which
    // network interface to use, for example, a corporate wired ethernet usually does not transmit UDP packets
    // to a wifi connected device, so in that case the localAddress needs to be the IP address of a specific wifi internet
    // adapter rather than 127.0.0.1.
    static std::shared_ptr<AdHocConnection> connectRemoteUdp(const std::string& nodeName, std::string localAddr, std::string remoteAddr, int remotePort);

    // instance methods
    bool isOpen();
    void close();

    // provide a callback function that will be called for every message "received" from the remote mavlink node.
    int subscribe(AdHocMessageHandler handler);
    void unsubscribe(int id);

    uint8_t getNextSequence();

    // Pack and send the given message, assuming the compid and sysid have been set by the caller.
    void sendMessage(const std::vector<uint8_t>& msg);

protected:
    void startListening(const std::string& nodeName, std::shared_ptr<Port> connectedPort);

public:
    //needed for piml pattern
    ~AdHocConnection();

private:
    std::unique_ptr<mavlinkcom_impl::AdHocConnectionImpl> pImpl;
    friend class MavLinkNode;
    friend class mavlinkcom_impl::AdHocConnectionImpl;
};
}

#endif
