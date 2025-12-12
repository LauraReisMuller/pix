// include/server/election.h
// Módulo ISOLADO para eleição de líder (Algoritmo Bully)
// Dependências: apenas protocol.h e tipos básicos

#ifndef ELECTION_H
#define ELECTION_H

#include "common/protocol.h"
#include "server/replication.h"
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <netinet/in.h>

#define HEARTBEAT_INTERVAL_MS 1000
#define LEADER_TIMEOUT_MS 3000
#define ELECTION_TIMEOUT_MS 2000

using namespace std;
using namespace chrono;

// Callback para notificar mudanças de estado
using ElectionCallback = function<void(int new_leader_id, bool i_am_leader)>;

enum ElectionState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

class ElectionManager {
private:
    // === Configuração deste servidor ===
    int my_id;
    uint32_t my_addr;
    uint16_t my_port;
    int sockfd;

    // === Réplicas conhecidas ===
    vector<ReplicaInfo> replicas;
    mutable mutex replicas_mutex;
    
    // === Estado da eleição ===
    atomic<ElectionState> state;
    atomic<int> current_leader_id;
    atomic<bool> election_in_progress;
    steady_clock::time_point last_heartbeat_from_leader;
    mutable mutex heartbeat_mutex;
    
    // === Controle de threads ===
    atomic<bool> running;
    thread heartbeat_thread;
    thread monitor_thread;
    
    // === Callback para notificar mudanças ===
    ElectionCallback on_leader_change;
    
    // === Métodos privados ===
    void heartbeatLoop();
    void monitorLoop();
    void startElection();
    void sendElectionMsg(int target_id);
    void sendOkMsg(int target_id);
    void announceCoordinator();
    void becomeLeader();
    void becomeFollower(int leader_id);
    bool hasLowerReplicas();
    void markReplicaDead(int replica_id);

public:
    ElectionManager()
        : my_id(-1), sockfd(-1), state(FOLLOWER),
          current_leader_id(0), election_in_progress(false), running(false) {}
    
    // === Interface pública ===
    
    // Configuração inicial
    void init (int socket, int id, bool is_leader);
    void addReplica(int id, string ip, int port);
    void setLeaderChangeCallback(ElectionCallback callback);
    
    // Controle do módulo
    void start();
    void stop();
    
    // Handlers de mensagens (chamados pelo main loop)
    void handleElectionMessage(const Packet& packet, const struct sockaddr_in& sender);
    void handleOkMessage(const Packet& packet, const struct sockaddr_in& sender);
    void handleCoordinatorMessage(const Packet& packet, const struct sockaddr_in& sender);
    void handleHeartbeatMessage(const Packet& packet, const struct sockaddr_in& sender);
    void handleHeartbeatAck(const Packet& packet, const struct sockaddr_in& sender);
    
    // Consultas de estado
    bool isLeader() const { return state == LEADER; }
    int getLeaderId() const { return current_leader_id; }
    int getMyId() const { return my_id; }
    ElectionState getState() const { return state; }
    
    // Forçar eleição (para testes ou detecção manual de falha)
    void triggerElection();
};

extern ElectionManager election_manager;

#endif // ELECTION_H