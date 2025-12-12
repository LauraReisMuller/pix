#ifndef SERVER_DISCOVERY_H
#define SERVER_DISCOVERY_H

#include <string>
#include <netinet/in.h>
#include "common/protocol.h" 
#include "server/database.h"
#include "common/utils.h"
#include "server/replication.h" 

class ServerDiscovery {
public:
    void sendDiscoveryAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen);
    void handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd);
};

#endif // SERVER_DISCOVERY_H