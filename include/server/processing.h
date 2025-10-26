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

class ServerProcessing {
public:
    /**
     * @brief Processa uma requisição recebida e envia ACK de volta usando um socket externo.
     * @param packet Pacote recebido do cliente
     * @param client_addr Endereço do cliente que enviou a requisição
     * @param clilen Tamanho da estrutura sockaddr_in
     * @param sockfd Socket externo a ser usado para enviar a resposta
     */
    void handleRequest(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd);
};


#endif // SERVER_PROCESSING_H