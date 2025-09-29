#ifndef SERVER_DISCOVERY_H
#define SERVER_DISCOVERY_H

#include <string>
#include <netinet/in.h>
#include "common/protocol.h" 

class ServerDiscovery {
public:

    ServerDiscovery(int port);
    void startDiscoveryListener();

private:
    int _port;
    int _sockfd;
    struct sockaddr_in _serv_addr;

    void setupSocket();
    
    // processa o pacote de descoberta recebido
    void handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen);
};

#endif // SERVER_DISCOVERY_H