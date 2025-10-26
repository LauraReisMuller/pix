/**
 * Implementação da descoberta do servidor (lado cliente).
 *
 * Envia pacotes de broadcast UDP com PKT_DISCOVER e aguarda respostas
 * do tipo PKT_DISCOVER_ACK.
 *
 * Constantes relevantes:
 *  - DISCOVERY_TIMEOUT_SEC: timeout por tentativa (em segundos)
 *  - MAX_DISCOVERY_ATTEMPTS: número máximo de retransmissões
 */

#include "client/discovery.h"
#include "common/protocol.h"
#include "common/utils.h"

#define DISCOVERY_TIMEOUT_SEC 1
#define MAX_DISCOVERY_ATTEMPTS 50


ClientDiscovery::ClientDiscovery(int port) : _port(port), _sockfd(-1) {
    memset(&_serv_addr, 0, sizeof(_serv_addr));
    _serv_addr.sin_family = AF_INET;
    _serv_addr.sin_port = htons(_port);

    _serv_addr.sin_addr.s_addr = inet_addr("255.255.255.255"); 
}

void ClientDiscovery::setupSocket() {
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0) {
        log_message("ERROR opening client socket");
        throw runtime_error("Failed to open socket.");
    }
    
    int enable = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
        log_message("ERROR setting SO_BROADCAST option");
        close(_sockfd);
        throw runtime_error("Failed to set SO_BROADCAST.");
    }
}

bool ClientDiscovery::waitForResponse(sockaddr_in& server_info, socklen_t& len) {
    fd_set read_fds;
    struct timeval tv;
    int retval;

    FD_ZERO(&read_fds);
    FD_SET(_sockfd, &read_fds);

    tv.tv_sec = DISCOVERY_TIMEOUT_SEC;
    tv.tv_usec = 0;

    retval = select(_sockfd + 1, &read_fds, NULL, NULL, &tv);

    if (retval == -1) {
        log_message("ERROR in select() during discovery");
        return false;
    } else if (retval == 0) {
        log_message("Discovery timeout. Retrying...");
        return false;
    } else {
        Packet response_packet;
        
        ssize_t n = recvfrom(_sockfd, (char*)&response_packet, sizeof(Packet), 0, 
                             (struct sockaddr*)&server_info, &len);
        
        if (response_packet.type != PKT_DISCOVER_ACK) {
            log_message("Received unexpected packet type during discovery. Ignoring.");
            return false;
        }
        
        if (n < 0) {
            log_message("ERROR on recvfrom during discovery");
            return false;
        }
        return true;
    }
}

string ClientDiscovery::discoverServer() {
    try {
        setupSocket();
    } catch (const runtime_error& e) {
        return ""; 
    }

    Packet discovery_packet;
    discovery_packet.type = PKT_DISCOVER;
    discovery_packet.seqn = 0;

    struct sockaddr_in server_info;
    socklen_t len = sizeof(server_info);

    for (int i = 0; i < MAX_DISCOVERY_ATTEMPTS; ++i) {
        
        ssize_t n = sendto(_sockfd, (const char*)&discovery_packet, sizeof(Packet), 0, 
                           (const struct sockaddr*)&_serv_addr, sizeof(_serv_addr));
        
        if (n < 0) {
            log_message("ERROR sending discovery broadcast");
            continue; 
        }
        if (waitForResponse(server_info, len)) {
    
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &server_info.sin_addr, ip_str, INET_ADDRSTRLEN);
            
            close(_sockfd);
            return string(ip_str);
        }
    }

    log_message("Failed to discover server after multiple attempts.");
    if (_sockfd >= 0) {
        close(_sockfd);
    }
    return "";
}
