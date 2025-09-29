// discovery.h
#ifndef CLIENT_DISCOVERY_H
#define CLIENT_DISCOVERY_H

#include <string>
#include <netinet/in.h>

class ClientDiscovery{
public:

    ClientDiscovery(int port);

    std::string discoverServer();

private:

    int _port;
    int _sockfd;
    struct sockaddr_in _serv_addr;

    void setupSocket();
    void enableBroadcast();
    bool waitForResponse(sockaddr_in& server_info, socklen_t& len);
};

#endif // CLIENT_DISCOVERY_H
