#ifndef SERVER_DISCOVERY_H
#define SERVER_DISCOVERY_H

#include <string>
#include <netinet/in.h>
#include "common/protocol.h" 

class ServerDiscovery {
public:
    
    // Processa o pacote de descoberta recebido usando um socket externo
    void handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd);
};

#endif // SERVER_DISCOVERY_H