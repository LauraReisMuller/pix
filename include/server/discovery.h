#ifndef SERVER_DISCOVERY_H
#define SERVER_DISCOVERY_H

#include <string>
#include <netinet/in.h>
#include "common/protocol.h" 

class ServerDiscovery {
public:
    void sendDiscoveryAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen);

    /**
     * @brief Processa o pacote de descoberta recebido usando um socket externo.
     * @param packet Pacote de descoberta recebido
     * @param client_addr Endereço do cliente que enviou o pacote
     * @param clilen Tamanho da estrutura sockaddr_in
     * @param sockfd Socket externo a ser usado para enviar a resposta
     */
    void handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd);
};

#endif // SERVER_DISCOVERY_H