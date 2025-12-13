#include "server/election.h"
#include "common/utils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <algorithm>
#include <sstream>

ElectionManager election_manager;

void ElectionManager::init(int socket, int id, bool is_leader) {
    this->sockfd = socket;
    this->my_id = id;
    
    if (is_leader) {
        state = LEADER;
        current_leader_id = my_id;
    } else {
        state = FOLLOWER;
        current_leader_id = 0; // Desconhecido inicialmente. Servidores começam como follower.
    }
    
    log_message(("ElectionManager initialized for server ID " + to_string(my_id)).c_str());
}

void ElectionManager::addReplica(int id, string ip, int port) {
    lock_guard<recursive_mutex> lock(replicas_mutex);
    
    // 1. VERIFICAÇÃO DE DUPLICIDADE
    // Percorre a lista para ver se esse ID já existe
    for (auto& replica : replicas) {
        if (replica.id == id) {
            // Já conhecemos esse servidor, apenas reativa ele.
            if (!replica.active) {
                replica.active = true;
                log_message(("Reactivated known replica ID " + to_string(id)).c_str());
            }
            return;
        }
    }
    
    ReplicaInfo replica;
    replica.id = id;
    replica.ip = ip;
    replica.port = port;
    replica.active = true;
    
    // Configura sockaddr_in
    memset(&replica.addr, 0, sizeof(replica.addr));
    replica.addr.sin_family = AF_INET;
    replica.addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &replica.addr.sin_addr);
    
    replicas.push_back(replica);
    
    log_message(("Added replica ID " + to_string(id) + " at " + ip + ":" + to_string(port)).c_str());
}

void ElectionManager::setLeaderChangeCallback(ElectionCallback callback) {
    on_leader_change = callback;
}

void ElectionManager::start() {
    if (running) {
        log_message("ElectionManager already running.");
        return;
    }
    
    running = true;
    
    // Servidor com menor ID é o líder inicial
    lock_guard<recursive_mutex> lock(replicas_mutex);
    int lowest_id;

    try {
        if (my_id < 0) {
            throw runtime_error("Server ID not set properly.");
        }
        lowest_id = my_id;
    } catch (const exception& e) {
        log_message(("ERROR: " + string(e.what())).c_str());
        return;
    }

    for (const auto& replica : replicas) {
        if (replica.id < lowest_id && replica.id >= 0) {
            lowest_id = replica.id;
        }
    }
    
    if (lowest_id == my_id) {
        becomeLeader();
    } else {
        current_leader_id = lowest_id;
        state = FOLLOWER;
        lock_guard<mutex> lock(heartbeat_mutex);
        last_heartbeat_from_leader = steady_clock::now();
        log_message(("Starting as FOLLOWER. Expected leader: " + to_string(lowest_id)).c_str());
    }
    
    // Inicia threads de hearbeat e de escuta de eleição
    heartbeat_thread = thread(&ElectionManager::heartbeatLoop, this);
    monitor_thread = thread(&ElectionManager::monitorLoop, this);
    
    log_message("ElectionManager started.");
}

void ElectionManager::stop() {
    if (!running) return;
    
    running = false;
    
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    log_message("ElectionManager stopped.");
}

// LOOPS DE THREADS
void ElectionManager::heartbeatLoop() {
    while (running) {
        if (state == LEADER) {
            // Líder envia heartbeats para todos
            lock_guard<recursive_mutex> lock(replicas_mutex);;
            
            Packet hb_packet;
            memset(&hb_packet, 0, sizeof(Packet));
            hb_packet.type = PKT_HEARTBEAT;
            hb_packet.heartbeat.sender_id = my_id;
            hb_packet.heartbeat.sender_addr = my_addr;
            hb_packet.heartbeat.sender_port = my_port;
            hb_packet.heartbeat.is_primary = 1;
            
            for (auto& replica : replicas) {
                ssize_t sent = sendto(sockfd, &hb_packet, sizeof(Packet), 0,
                                     (struct sockaddr*)&replica.addr, sizeof(replica.addr));
                if (sent < 0) {
                    log_message(("Failed to send heartbeat to replica " + to_string(replica.id)).c_str());
                }
            }
        }
        
        this_thread::sleep_for(milliseconds(HEARTBEAT_INTERVAL_MS));
    }
}

// Monitora heartbeats, pode inciar eleição e se declara líder de acordo.
void ElectionManager::monitorLoop() {    
    while (running) {
        if (state == FOLLOWER) {
            steady_clock::time_point last_heartbeat;
            {
                lock_guard<mutex> lock(heartbeat_mutex);
                last_heartbeat = last_heartbeat_from_leader;
            }
            
            auto elapsed = duration_cast<milliseconds>(steady_clock::now() - last_heartbeat).count();

            if (elapsed > LEADER_TIMEOUT_MS) {
                log_message("Leader timeout detected. Starting election...");
                startElection();
                last_heartbeat_from_leader = steady_clock::now(); // Reset
            }
        } else if (state == CANDIDATE) {
            if (election_in_progress) {
                // Timeout de eleição, assume vitória
                this_thread::sleep_for(milliseconds(ELECTION_TIMEOUT_MS));

                if (election_in_progress) {
                    log_message("Election timeout. No lower process responded. Becoming leader.");
                    becomeLeader();
                }
            }
        } else {
            // LEADER - reseta timeout
            last_heartbeat_from_leader = steady_clock::now();
        }
        
        this_thread::sleep_for(milliseconds(500));
    }
}

// ELEIÇÃO COM ALGORITMO VALENTÃO
void ElectionManager::startElection() {
    if (election_in_progress) {
        log_message("Election already in progress. Ignoring.");
        return;
    }
    
    log_message("Starting election...");
    state = CANDIDATE;
    election_in_progress = true;
    
    bool found_lower = false;
    
    {
        lock_guard<recursive_mutex> lock(replicas_mutex);;
        
        // Envia ELECTION para todos os processos com ID menor
        for (auto& replica : replicas) {
            if (replica.id < my_id && replica.active) {
                sendElectionMsg(replica.id);
                found_lower = true;
            }
        }
    }
    
    if (!found_lower) {
        // Não há processos maiores, torno-me líder imediatamente
        log_message("No lower processes found. Becoming leader immediately.");
        becomeLeader();
    }
}

void ElectionManager::sendElectionMsg(int target_id) {
    lock_guard<recursive_mutex> lock(replicas_mutex);;
    
    auto it = find_if(replicas.begin(), replicas.end(),
                      [target_id](const ReplicaInfo& r) { return r.id == target_id; });
    
    if (it == replicas.end()) {
        log_message(("Replica ID " + to_string(target_id) + " not found.").c_str());
        return;
    }
    
    Packet election_packet;
    memset(&election_packet, 0, sizeof(Packet));
    election_packet.type = PKT_ELECTION;
    election_packet.election.candidate_id = my_id;
    election_packet.election.candidate_addr = my_addr;
    election_packet.election.candidate_port = my_port;
    
    ssize_t sent = sendto(sockfd, &election_packet, sizeof(Packet), 0,
                         (struct sockaddr*)&it->addr, sizeof(it->addr));
    
    if (sent < 0) {
        log_message(("Failed to send ELECTION to " + to_string(target_id)).c_str());
        markReplicaDead(target_id);
    } else {
        log_message(("Sent ELECTION to replica " + to_string(target_id)).c_str());
    }
}

void ElectionManager::sendOkMsg(int target_id) {
    lock_guard<recursive_mutex> lock(replicas_mutex);;
    
    auto it = find_if(replicas.begin(), replicas.end(),
                      [target_id](const ReplicaInfo& r) { return r.id == target_id; });
    
    if (it == replicas.end()) return;
    
    Packet ok_packet;
    memset(&ok_packet, 0, sizeof(Packet));
    ok_packet.type = PKT_ELECTION_OK;
    ok_packet.election_ok.responder_id = my_id;
    ok_packet.election_ok.responder_addr = my_addr;
    ok_packet.election_ok.responder_port = my_port;
    
    ssize_t sent = sendto(sockfd, &ok_packet, sizeof(Packet), 0,
                         (struct sockaddr*)&it->addr, sizeof(it->addr));
    
    if (sent < 0) {
        log_message(("Failed to send OK to " + to_string(target_id)).c_str());
    } else {
        log_message(("Sent ELECTION_OK to replica " + to_string(target_id)).c_str());
    }
}

void ElectionManager::announceCoordinator() {
    lock_guard<recursive_mutex> lock(replicas_mutex);;
    
    Packet coord_packet;
    memset(&coord_packet, 0, sizeof(Packet));
    coord_packet.type = PKT_COORDINATOR;
    coord_packet.coordinator.coordinator_id = my_id;
    coord_packet.coordinator.coordinator_addr = my_addr;
    coord_packet.coordinator.coordinator_port = my_port;
    
    // Envia para todos (broadcast)
    for (auto& replica : replicas) {
        ssize_t sent = sendto(sockfd, &coord_packet, sizeof(Packet), 0,
                             (struct sockaddr*)&replica.addr, sizeof(replica.addr));
        if (sent < 0) {
            log_message(("Failed to announce COORDINATOR to " + to_string(replica.id)).c_str());
        }
    }
    
    log_message("Announced COORDINATOR to all replicas.");
}

void ElectionManager::becomeLeader() {
    if (state == LEADER) return;
    
    state = LEADER;
    current_leader_id = my_id;
    election_in_progress = false;
    
    log_message_core(("This instance is now the LEADER (ID " + to_string(my_id) + ")").c_str());

    announceCoordinator();
    // Notifica via callback
    if (on_leader_change) {
        on_leader_change(my_id, true);
    }
}

void ElectionManager::becomeFollower(int leader_id) {
    if (state == FOLLOWER && current_leader_id == leader_id) return;
    
    log_message(("Becoming FOLLOWER. New leader: " + to_string(leader_id)).c_str());
    state = FOLLOWER;
    current_leader_id = leader_id;
    election_in_progress = false;
    
    // Notifica via callback
    if (on_leader_change) {
        on_leader_change(leader_id, false);
    }
}

bool ElectionManager::hasLowerReplicas() {
    lock_guard<recursive_mutex> lock(replicas_mutex);;
    
    for (const auto& replica : replicas) {
        if (replica.id > my_id && replica.active) {
            return true;
        }
    }
    return false;
}

void ElectionManager::markReplicaDead(int replica_id) {
    lock_guard<recursive_mutex> lock(replicas_mutex);;
    
    auto it = find_if(replicas.begin(), replicas.end(),
                      [replica_id](const ReplicaInfo& r) { return r.id == replica_id; });
    
    if (it != replicas.end()) {
        it->active = false;
        log_message(("Marked replica " + to_string(replica_id) + " as DEAD.").c_str());
    }
}

// HANDLERS DE MENSAGENS
void ElectionManager::handleElectionMessage(const Packet& packet, const struct sockaddr_in& sender) {
    int candidate_id = packet.election.candidate_id;
    
    log_message(("Received ELECTION from " + to_string(candidate_id)).c_str());
    
    // Servidor candidato pior do que esse
    if (my_id < candidate_id) {
        if (state == LEADER) { 
             // Já é líder e um subordinado tentou eleição, manda COORDINATOR novamente.
             log_message(("Subordinate " + to_string(candidate_id) + " tried election. Re-asserting authority.").c_str());
             announceCoordinator(); 
        } else {
             // Responde OK e propaga Election para tentar virar líder
             sendOkMsg(candidate_id);
             if (!election_in_progress) startElection();
        }
    }
}

void ElectionManager::handleOkMessage(const Packet& packet, const struct sockaddr_in& sender) {
    int responder_id = packet.election_ok.responder_id;
    
    log_message(("Received ELECTION_OK from " + to_string(responder_id)).c_str());
    
    // Alguém com id menor respondeu, então não é líder
    if (state == CANDIDATE) {
        log_message("Lower process responded. Stepping down from candidacy.");
        state = FOLLOWER;
        election_in_progress = false;
    }
}

void ElectionManager::handleCoordinatorMessage(const Packet& packet, const struct sockaddr_in& sender) {
    int coordinator_id = packet.coordinator.coordinator_id;
    
    log_message(("Received COORDINATOR announcement. New leader: " + to_string(coordinator_id)).c_str());
    
    if (coordinator_id != my_id) {
        becomeFollower(coordinator_id);
    }
}

void ElectionManager::handleHeartbeatMessage(const Packet& packet, const struct sockaddr_in& sender) {
    int sender_id = packet.heartbeat.sender_id;
    bool sender_is_primary = packet.heartbeat.is_primary;
    
    if (sender_is_primary) {
        {
            lock_guard<mutex> lock(heartbeat_mutex);
            last_heartbeat_from_leader = steady_clock::now();
        }

        // Atualiza líder e marca como ativo
        if (current_leader_id != sender_id) {
            log_message(("Received heartbeat from new leader: " + to_string(sender_id)).c_str());
            becomeFollower(sender_id);
        }
        
        // Marca réplica como ativa
        lock_guard<recursive_mutex> lock(replicas_mutex);;
        auto it = find_if(replicas.begin(), replicas.end(),
                          [sender_id](const ReplicaInfo& r) { return r.id == sender_id; });
        if (it != replicas.end()) {
            it->active = true;
        }
        
        // Envia ACK
        Packet ack_packet;
        memset(&ack_packet, 0, sizeof(Packet));
        ack_packet.type = PKT_HEARTBEAT_ACK;
        ack_packet.heartbeat.sender_id = my_id;
        ack_packet.heartbeat.sender_addr = my_addr;
        ack_packet.heartbeat.sender_port = my_port;
        ack_packet.heartbeat.is_primary = (state == LEADER) ? 1 : 0;
        
        sendto(sockfd, &ack_packet, sizeof(Packet), 0,
               (struct sockaddr*)&sender, sizeof(sender));
    }
}

void ElectionManager::handleHeartbeatAck(const Packet& packet, const struct sockaddr_in& sender) {
    int sender_id = packet.heartbeat.sender_id;
    
    // Marca réplica como ativa
    lock_guard<recursive_mutex> lock(replicas_mutex);;
    auto it = find_if(replicas.begin(), replicas.end(),
                      [sender_id](const ReplicaInfo& r) { return r.id == sender_id; });
    if (it != replicas.end()) {
        it->active = true;
    }
}

// Eleição manual para teste.
void ElectionManager::triggerElection() {
    log_message("Manual election trigger requested.");
    startElection();
}