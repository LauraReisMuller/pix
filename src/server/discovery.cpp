#include "server/discovery.h"
#include "common/protocol.h"
#include "server/database.h"
#include "common/utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <stdexcept>

void ServerDiscovery::handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
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
    
    ssize_t n = sendto(sockfd, (const char*)&discovery_ack, sizeof(Packet), 0, 
                       (const struct sockaddr *) &client_addr, clilen);
    
    if (n < 0) {
        log_message("ERROR on sendto discovery ACK");
    } else {
        string ack_msg = "Sent ACK to discovered client: " + string(client_ip);
        log_message(ack_msg.c_str());
    }
}