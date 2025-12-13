#ifndef REPLICATION_H
#define REPLICATION_H

#include <vector>
#include <string>
#include <netinet/in.h>
#include "common/protocol.h"
#include "server/database.h"
#include "server/interface.h"
#include "common/utils.h"
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

// Estrutura para guardar info das outras réplicas
struct ReplicaInfo {
    int id;
    string ip;
    int port;
    struct sockaddr_in addr;
    bool active;
};

class ReplicationManager {
private:
    vector<ReplicaInfo> replicas;
    int my_id;
    int sockfd; // Socket UDP compartilhado
    bool is_leader_flag;
    mutable std::mutex replicas_mutex;

public:
    ReplicationManager();
    
    // Inicializa com ID e Socket
    void init(int socket, int id, bool leader_status);
    
    // MUDAR DEPOIS!!! Adiciona servidores conhecidos (hardcoded por enquanto)
    void addReplica(int id, string ip, int port);

    // Getters/Setters
    bool isLeader() const { return is_leader_flag; }
    void setLeader(bool status) { is_leader_flag = status; }

    // [LÍDER] Tenta replicar para os backups e espera ACK
    bool replicateState(const string& origin_ip, const string& dest_ip, 
                                        uint32_t amount, uint32_t seqn,
                                        uint32_t final_bal_orig, uint32_t final_bal_dest);
    bool replicateNewClient(const string& client_ip);

    bool replicateQuery(const string &client_ip, uint32_t seqn);

    // [BACKUP] Recebe ordem do líder e aplica no DB
    void handleReplicationMessage(const Packet& pkt, const struct sockaddr_in& sender_addr);
};

extern ReplicationManager replication_manager;

#endif