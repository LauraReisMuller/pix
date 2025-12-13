#include "server/discovery.h"

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
    
    sendDiscoveryAck(sockfd, client_addr, clilen);

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
}

void ServerDiscovery::sendServerBroadcast(int sockfd, int my_id, int my_replica_port) {
    // 1. Habilita Broadcast no socket
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        log_message("ERROR setting socket to broadcast mode");
        return;
    }

    // 2. Monta o pacote
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PKT_SERVER_DISCOVER;
    pkt.seqn = 0;
    pkt.server_discovery.id = my_id;
    pkt.server_discovery.replica_port = my_replica_port;

    // 3. Configura endereço de destino
    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(my_replica_port); // Porta 5000 (todos escutam nela)
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // 4. Envia
    ssize_t sent = sendto(sockfd, &pkt, sizeof(Packet), 0, 
                         (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

    if (sent < 0) {
        log_message("ERROR sending server discovery broadcast");
    } else {
        log_message("Sent SERVER_DISCOVERY broadcast.");
    }
}