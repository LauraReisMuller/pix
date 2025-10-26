#include "server/discovery.h"
#include "common/protocol.h"
#include "server/database.h"
#include "common/utils.h"

void ServerDiscovery::handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
    //Verificar o tipo de pacote
    if (packet.type != PKT_DISCOVER) {
        // Ignora pacotes que não sejam de descoberta neste listener (ex: REQ)
        return; 
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    string client_key = string(client_ip);
    server_db.addClient(client_key);
    
    /* Log de teste.
    string log_msg = "Received DISCOVERY from client: " + string(client_ip);
    server_db.getClient(client_key);
    log_message(log_msg.c_str());
    */
    
    // Criar um pacote ACK/Resposta para Descoberta
    Packet discovery_ack;
    discovery_ack.type = PKT_DISCOVER_ACK;
    discovery_ack.seqn = 0; 
    
    ssize_t n = sendto(sockfd, (const char*)&discovery_ack, sizeof(Packet), 0, 
                       (const struct sockaddr *) &client_addr, clilen);
    
    if (n < 0) {
        log_message("ERROR on sendto discovery ACK");
    }
}