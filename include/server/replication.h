#ifndef REPLICATION_H
#define REPLICATION_H

#include <vector>
#include <string>
#include <netinet/in.h>
#include "../common/protocol.h"

// Estrutura para guardar info das outras réplicas
struct ReplicaInfo {
    int id;
    std::string ip;
    int port;
    struct sockaddr_in addr;
    bool active;
};

class ReplicationManager {
private:
    std::vector<ReplicaInfo> replicas;
    int my_id;
    int sockfd; // Socket UDP compartilhado
    bool is_leader_flag;

public:
    ReplicationManager();
    
    // Inicializa com ID e Socket
    void init(int socket, int id, bool leader_status);
    
    // MUDAR DEPOIS!!! Adiciona servidores conhecidos (hardcoded por enquanto)
    void addReplica(int id, std::string ip, int port);

    // Getters/Setters
    bool isLeader() const { return is_leader_flag; }
    void setLeader(bool status) { is_leader_flag = status; }

    // [LÍDER] Tenta replicar para os backups e espera ACK
    bool replicateTransaction(const std::string& origin_ip, const std::string& dest_ip, uint32_t amount, uint32_t seqn);
    bool replicateNewClient(const std::string& client_ip);

    // [BACKUP] Recebe ordem do líder e aplica no DB
    void handleReplicationMessage(const Packet& pkt, const struct sockaddr_in& sender_addr);
};

#endif