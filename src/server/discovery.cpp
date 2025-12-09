#include "server/discovery.h"
#include "common/protocol.h"
#include "server/database.h"
#include "common/utils.h"
#include "server/replication.h" 

extern ReplicationManager replication_manager;

void ServerDiscovery::sendDiscoveryAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen) {
    Packet discovery_ack;
    discovery_ack.type = PKT_DISCOVER_ACK;
    discovery_ack.seqn = 0; 
    
    ssize_t sent_bytes = sendto(sockfd, (const char*)&discovery_ack, sizeof(Packet), 0, 
                       (const struct sockaddr *) &client_addr, clilen);

    if (sent_bytes < 0) {
        log_message("ERROR on sendto discovery ACK");
    }
}

void ServerDiscovery::handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    string client_key = string(client_ip);
    
    // [NOVO] Replica a criação do cliente para os Backups
    // Se falhar a replicação, tecnicamente deveríamos falhar a descoberta,
    // mas para simplificar, vamos apenas logar o erro.
    bool replicated = replication_manager.replicateNewClient(client_key);
    
    if (!replicated) {
        log_message("AVISO: Falha ao replicar novo cliente para backups.");
    }

    // Aplica localmente (Líder)
    server_db.addClient(client_key);
    server_db.updateBankSummary();
    
    sendDiscoveryAck(sockfd, client_addr, clilen);
}