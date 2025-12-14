#ifndef SERVER_PROCESSING_H
#define SERVER_PROCESSING_H

#include <sys/socket.h>
#include <netinet/in.h>
#include "common/protocol.h"
#include "common/utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdio.h>
#include <thread>
#include "server/database.h"
#include "server/interface.h"
#include "common/protocol.h" 
#include "common/utils.h"
#include "server/replication.h"    
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <cstring> 

class ServerProcessing {
public:
    void handleRequest(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd);
};


#endif // SERVER_PROCESSING_H