#include "server/discovery.h"
#include "common/protocol.h"
#include "server/databases.h"
#include "common/utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <stdexcept>


/**
 * @brief Construtor da classe ServerDiscovery.
 * @param port A porta UDP que o servidor irá escutar.
 */
ServerDiscovery::ServerDiscovery(int port) : _port(port), _sockfd(-1) {
    // Inicializa a estrutura de endereço do servidor
    memset(&_serv_addr, 0, sizeof(_serv_addr));
    _serv_addr.sin_family = AF_INET;
    _serv_addr.sin_port = htons(_port);

    // Escuta em qualquer interface de rede
    _serv_addr.sin_addr.s_addr = INADDR_ANY; 
}

/**
 * @brief Cria e configura o socket UDP, realizando o bind.
 */
void ServerDiscovery::setupSocket() {
    // Criação do socket
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0) {
        log_message("ERROR opening server socket");
        throw runtime_error("Failed to open socket.");
    }

    // O servidor deve conseguir receber mensagens broadcast (embora seja passivo)
    int optval = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
    
    // Bind do socket com o endereço e porta do servidor
    if (bind(_sockfd, (struct sockaddr *) &_serv_addr, sizeof(struct sockaddr)) < 0) {
        log_message("ERROR on binding server socket");
        close(_sockfd);
        throw runtime_error("Failed to bind socket.");
    }
    log_message("Server socket bound and listening for discovery messages.");
}

/**
 * @brief Envia uma resposta unicast para o cliente que enviou o broadcast.
 * @param client_addr Endereço do cliente obtido via recvfrom.
 */
void ServerDiscovery::handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen) {
    
    //Verificar o tipo de pacote
    if (packet.type != PKT_DISCOVER) {
        // Ignora pacotes que não sejam de descoberta neste listener (ex: REQ)
        return; 
    }

    // NOTA: Registra o novo cliente na tabela (Lógica do Servidor a ser implementada! - Gui e Max)
    //... registra o cliente, inicializa o saldo (100 reais) e o last_req=0.
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    string client_key = string(client_ip);
    server_db.addClient(client_key);
    
    // Log de teste.
    string log_msg = "Received DISCOVERY from client: " + string(client_ip);
    server_db.getClient(client_key);
    
    log_message(log_msg.c_str());

    //Resposta Unicast: Confirma o endereço do servidor para o cliente
    
    // Criar um pacote ACK/Resposta para Descoberta
    Packet discovery_ack;
    discovery_ack.type = PKT_DISCOVER_ACK;
    discovery_ack.seqn = 0; 
    
    ssize_t n = sendto(_sockfd, (const char*)&discovery_ack, sizeof(Packet), 0, 
                       (const struct sockaddr *) &client_addr, clilen);
    
    if (n < 0) {
        log_message("ERROR on sendto discovery ACK");
    } else {
        string ack_msg = "Sent ACK to discovered client: " + string(client_ip);
        log_message(ack_msg.c_str());
    }
}

/**
 * @brief Loop principal de escuta para mensagens de Descoberta.
 */
void ServerDiscovery::startDiscoveryListener() {

    try {
        setupSocket();
    } catch (const runtime_error& e) {
        return; 
    }

    Packet received_packet;
    struct sockaddr_in client_addr;
    socklen_t clilen = sizeof(client_addr);

    while (true) {
        memset(&received_packet, 0, sizeof(Packet));
        
        // recvfrom recebe o pacote e preenche o endereço do remetente (cliente)
        ssize_t n = recvfrom(_sockfd, (char*)&received_packet, sizeof(Packet), 0, 
                             (struct sockaddr *) &client_addr, &clilen);
        
        if (n < 0) {
            log_message("ERROR on recvfrom in discovery listener");
            continue; // Tenta receber o próximo pacote
        }
        
        // Processa a mensagem e envia o ACK (se for uma Descoberta)
        handleDiscovery(received_packet, client_addr, clilen);
    }
    
    close(_sockfd);
}