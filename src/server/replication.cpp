#include "../../include/server/replication.h"
#include "../../include/server/database.h"
#include "../../include/common/utils.h"
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>

// Referência ao banco de dados global (definido em database.cpp)
extern ServerDatabase server_db;

ReplicationManager::ReplicationManager() : my_id(-1), sockfd(-1), is_leader_flag(false) {}

void ReplicationManager::init(int socket, int id, bool leader_status) {
    this->sockfd = socket;
    this->my_id = id;
    this->is_leader_flag = leader_status;

    // --- CONFIGURAÇÃO HARDCODED (Até o Max fazer a Descoberta) ---
    // Ajuste esses IPs/Portas conforme o seu docker-compose ou teste local
    if (my_id == 0) { // Assumindo Líder na porta 4000
        addReplica(1, "127.0.0.1", 4001); // Backup 1
        // addReplica(2, "127.0.0.1", 4002); // Backup 2 (se houver)
    } else {
        addReplica(0, "127.0.0.1", 4000); // Líder
    }
}

void ReplicationManager::addReplica(int id, std::string ip, int port) {
    ReplicaInfo r;
    r.id = id; r.ip = ip; r.port = port; r.active = true;
    memset(&r.addr, 0, sizeof(r.addr));
    r.addr.sin_family = AF_INET;
    r.addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &r.addr.sin_addr);
    replicas.push_back(r);
}

// --- LÓGICA DO LÍDER ---
bool ReplicationManager::replicateTransaction(const std::string& origin_ip, const std::string& dest_ip, uint32_t amount, uint32_t seqn) {
    if (!is_leader_flag) return false;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PKT_REPLICATION_REQ;
    pkt.seqn = seqn; // Importante para consistência e deduplicação
    
    // Converte Strings para Uint32 para caber no pacote
    pkt.rep.origin_addr = ipToUint32(origin_ip);
    pkt.rep.dest_addr   = ipToUint32(dest_ip);
    pkt.rep.value       = amount;

    int sent_count = 0;
    // Envia para todas as réplicas
    for (const auto& r : replicas) {
        if (r.active) {
            sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr*)&r.addr, sizeof(r.addr));
            sent_count++;
        }
    }

    if (sent_count == 0) return true; // Sem réplicas? Sucesso imediato (modo degradado)

    // --- ESPERA POR ACKS (Síncrono com Timeout) ---
    // esperar pelo menos 1 ACK.
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000; // 200ms timeout
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int acks_received = 0;
    Packet response;
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    // Tenta ler respostas
    // Loop simples: tenta ler N vezes ou até dar timeout
    for(int i=0; i < sent_count * 2; i++) { 
        ssize_t n = recvfrom(sockfd, &response, sizeof(Packet), 0, (struct sockaddr*)&from_addr, &len);
        
        if (n > 0 && response.type == PKT_REPLICATION_ACK && response.seqn == seqn) {
            acks_received++;
            if (acks_received >= 1) break; 
        }
    }

    // Restaura socket para bloqueante (ou padrão anterior)
    struct timeval tv_def = {0, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_def, sizeof(tv_def));

    return (acks_received >= 1);
}

bool ReplicationManager::replicateNewClient(const std::string& client_ip) {
    if (!is_leader_flag) return false;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PKT_REP_CLIENT_REQ;
    pkt.seqn = 0; // ID irrelevante para criação
    
    // Reaproveitamos a struct ReplicationData!
    // Usamos o campo 'origin_addr' para guardar o IP do novo cliente
    pkt.rep.origin_addr = ipToUint32(client_ip);
    pkt.rep.dest_addr = 0; // Não usado
    pkt.rep.value = 0;     // O valor inicial é padrão no DB, não precisa mandar

    // ... (Copie a mesma lógica de envio e espera por ACK do replicateTransaction aqui) ...
    // A única diferença é que você espera por PKT_REP_CLIENT_ACK
    
    // Exemplo simplificado de envio:
    for (const auto& r : replicas) {
        if (r.active) {
            sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr*)&r.addr, sizeof(r.addr));
        }
    }
    // (Implemente a espera pelo ACK igual ao transaction)
    return true; 
}

// --- LÓGICA DO BACKUP ---
void ReplicationManager::handleReplicationMessage(const Packet& pkt, const struct sockaddr_in& sender_addr) {
    
    if (pkt.type == PKT_REP_CLIENT_REQ) {
        std::string client_ip = uint32ToIp(pkt.rep.origin_addr);
        
        // Aplica no DB Local do Backup
        server_db.addClient(client_ip);
        server_db.updateBankSummary();

        // Envia ACK de volta
        Packet ack;
        ack.type = PKT_REP_CLIENT_ACK;
        sendto(sockfd, &ack, sizeof(Packet), 0, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
    }

    if (pkt.type != PKT_REPLICATION_REQ) return;

    // 1. Converte dados de volta (Uint32 -> String)
    std::string origin_ip = uint32ToIp(pkt.rep.origin_addr);
    std::string dest_ip   = uint32ToIp(pkt.rep.dest_addr);

    // 2. Aplica no DB Local
    // Criamos um packet fake para aproveitar a função makeTransaction existente
    Packet fake_req;
    fake_req.seqn = pkt.seqn;
    fake_req.req.value = pkt.rep.value;
    // Precisamos garantir que a função use o IP que passamos, e não tente extrair do fake_req
    
    // NOTA: O server_db.makeTransaction usa strings para IDs.
    // Ele executa validações de saldo. Como o Líder já validou, deve passar.
    // Se falhar (ex: saldo insuficiente localmente), temos uma inconsistência de estado.
    bool success = server_db.makeTransaction(origin_ip, dest_ip, fake_req);

    if (success) {
        // Adicionando informações visuais para debug
        string msg = "Backup: Transação replicada! Origem: " + origin_ip + 
                     " -> Destino: " + dest_ip + 
                     " Valor: " + to_string(pkt.rep.value);
        log_message(msg.c_str());
    } else {
        log_message("Backup ERRO: Falha ao aplicar transação replicada (Inconsistência?)");
    }

    // 3. Envia ACK para o Líder
    Packet ack;
    memset(&ack, 0, sizeof(Packet));
    ack.type = PKT_REPLICATION_ACK;
    ack.seqn = pkt.seqn; // Confirma o ID recebido

    sendto(sockfd, &ack, sizeof(Packet), 0, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
}