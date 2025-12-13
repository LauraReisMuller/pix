#include "server/replication.h"

ReplicationManager replication_manager;

ReplicationManager::ReplicationManager() : my_id(-1), sockfd(-1), is_leader_flag(false) {}

void ReplicationManager::init(int socket, int id, bool is_leader)
{
    this->sockfd = socket;
    this->my_id = id;
    this->is_leader_flag = is_leader;

    /*
    // --- CONFIGURAÇÃO HARDCODED (Até o Max fazer a Descoberta) ---
    // Ajuste esses IPs/Portas conforme o seu docker-compose ou teste local
    if (my_id == 0) { // Assumindo Líder na porta 4000
        addReplica(1, "127.0.0.1", 4001); // Backup 1
        // addReplica(2, "127.0.0.1", 4002); // Backup 2 (se houver)
    } else {
        addReplica(0, "127.0.0.1", 4000); // Líder
    }
    */
}

void ReplicationManager::addReplica(int id, string ip, int port)
{
    // [NOVO] Protege a lista
    lock_guard<mutex> lock(replicas_mutex);

    // 1. VERIFICAÇÃO DE DUPLICIDADE
    for (const auto &r : replicas)
    {
        if (r.id == id)
        {
            // Já existe, não faz nada
            return;
        }
    }

    // 2. ADIÇÃO
    ReplicaInfo r;
    r.id = id;
    r.ip = ip;
    r.port = port;
    r.active = true;

    memset(&r.addr, 0, sizeof(r.addr));
    r.addr.sin_family = AF_INET;
    r.addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &r.addr.sin_addr);

    replicas.push_back(r);

    // cout << "ReplicationManager: Added replica " << id << endl;
}

// --- LÓGICA DO LÍDER ---
bool ReplicationManager::replicateState(const string& origin_ip, const string& dest_ip, 
                                        uint32_t amount, uint32_t seqn,
                                        uint32_t final_bal_orig, uint32_t final_bal_dest) {
    if (!is_leader_flag) return false;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PKT_REPLICATION_REQ;
    pkt.seqn = seqn;
    pkt.rep.origin_addr = ipToUint32(origin_ip);
    pkt.rep.dest_addr   = ipToUint32(dest_ip);
    pkt.rep.value       = amount;
    
    // [NOVO] Enviando o gabarito
    pkt.rep.final_balance_origin = final_bal_orig;
    pkt.rep.final_balance_dest   = final_bal_dest;

    int sent_count = 0;
    // Envia para todas as réplicas
    for (const auto &r : replicas)
    {
        if (r.active)
        {
            sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&r.addr, sizeof(r.addr));
            sent_count++;
        }
    }
    cout << " => Sent_count: " << sent_count << endl;

    if (sent_count <= 1)
        return true; // Sem réplicas? Sucesso imediato (modo degradado)

    int acks_received = 0;
    Packet response;
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    // Configura o timeout para o select (200ms)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    // Configura o conjunto de descritores para o select
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // O select espera até que o socket tenha dados para ler OU o tempo acabe.
    // Ele NÃO altera o socket globalmente, então a Main Thread não sofre.
    int rv = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (rv > 0)
    {
        // Chegou dados! Tenta ler sem bloquear (MSG_DONTWAIT)
        // Usamos MSG_DONTWAIT para garantir que não vamos travar se a main thread ler o pacote antes de nós.
        ssize_t n = recvfrom(sockfd, &response, sizeof(Packet), MSG_DONTWAIT, (struct sockaddr *)&from_addr, &len);

        if (n > 0 && response.type == PKT_REPLICATION_ACK && response.seqn == seqn)
        {
            acks_received++;
        }
    }
    // Se rv == 0, foi timeout do select. Se rv < 0, foi erro do select.

    return (acks_received >= 1);
}

bool ReplicationManager::replicateNewClient(const string &client_ip)
{
    if (!is_leader_flag)
        return false;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PKT_REP_CLIENT_REQ;
    pkt.seqn = 0; // ID irrelevante para criação

    // Reaproveitamos a struct ReplicationData!
    // Usamos o campo 'origin_addr' para guardar o IP do novo cliente
    pkt.rep.origin_addr = ipToUint32(client_ip);
    pkt.rep.dest_addr = 0; // Não usado
    pkt.rep.value = 0;     // O valor inicial é padrão no DB, não precisa mandar

    int sent_count = 0;

    // Envia para todas as réplicas (Lógica idêntica ao replicateTransaction)
    for (const auto &r : replicas)
    {
        if (r.active)
        {
            sendto(sockfd, &pkt, sizeof(Packet), 0, (struct sockaddr *)&r.addr, sizeof(r.addr));
            sent_count++;
        }
    }

    if (sent_count == 0)
        return true; // Sem réplicas? Sucesso imediato

    // Configura a espera pelo ACK
    int acks_received = 0;
    Packet response;
    struct sockaddr_in from_addr;
    socklen_t len = sizeof(from_addr);

    // Configura o timeout para o select (200ms)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    // Configura o conjunto de descritores para o select
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // O select espera até que o socket tenha dados para ler OU o tempo acabe.
    int rv = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (rv > 0)
    {
        // Chegou dados! Tenta ler sem bloquear (MSG_DONTWAIT)
        ssize_t n = recvfrom(sockfd, &response, sizeof(Packet), MSG_DONTWAIT, (struct sockaddr *)&from_addr, &len);

        // AQUI MUDA: Verificamos se é o ACK de CRIAÇÃO DE CLIENTE (PKT_REP_CLIENT_ACK)
        if (n > 0 && response.type == PKT_REP_CLIENT_ACK)
        {
            acks_received++;
        }
    }

    return (acks_received >= 1);
}

// --- LÓGICA DO BACKUP ---
void ReplicationManager::handleReplicationMessage(const Packet &pkt, const struct sockaddr_in &sender_addr)
{

    cout << "[DEBUG] Backup recebeu pacote tipo: " << pkt.type << endl;

    if (pkt.type == PKT_REP_CLIENT_REQ)
    {
        string client_ip = uint32ToIp(pkt.rep.origin_addr);

        // Aplica no DB Local do Backup
        server_db.addClient(client_ip);
        server_db.updateBankSummary();
        cout << "[DEBUG] Criando cliente replicado..." << endl;
        // Envia ACK de volta
        Packet ack;
        ack.type = PKT_REP_CLIENT_ACK;
        sendto(sockfd, &ack, sizeof(Packet), 0, (struct sockaddr *)&sender_addr, sizeof(sender_addr));
    }

    if (pkt.type != PKT_REPLICATION_REQ)
        return;

    // 1. Converte dados de volta (Uint32 -> String)
    string origin_ip = uint32ToIp(pkt.rep.origin_addr);
    string dest_ip = uint32ToIp(pkt.rep.dest_addr);

    cout << "[DEBUG] Atualizacao de transacao de " << origin_ip << " para " << dest_ip << endl;

    // Verifica se já processamos essa requisição antes de tentar aplicar no banco
    uint32_t last_processed = server_db.getClientLastReq(origin_ip);

    if (pkt.seqn <= last_processed)
    {
        // Já processamos! Não executamos makeTransaction de novo.
        // Apenas enviamos o ACK novamente para garantir que o Líder receba.
        // [Opcional] cout << "[DEBUG] Backup ignorando duplicata ID " << pkt.seqn << endl;

        log_message("Mensagem de atualização duplicada. Atualização já efetuada");
        Packet ack;
        memset(&ack, 0, sizeof(Packet));
        ack.type = PKT_REPLICATION_ACK;
        ack.seqn = pkt.seqn;
        sendto(sockfd, &ack, sizeof(Packet), 0, (struct sockaddr *)&sender_addr, sizeof(sender_addr));
        return;
    }

    // --- APLICAÇÃO PASSIVA DO ESTADO ---
    // O Backup não usa "makeTransaction" (que calcula). 
    // Ele usa "applyState" (que obedece).
    
    // Você precisará criar esse método simples no Database ou usar os setters unsafe
    server_db.forceClientBalance(origin_ip, pkt.rep.final_balance_origin);
    server_db.forceClientBalance(dest_ip, pkt.rep.final_balance_dest);
    
    // Adiciona no histórico apenas para registro
    server_db.addTransaction_unsafe(origin_ip, pkt.seqn, dest_ip, pkt.rep.value);
    
    // Atualiza last_req
    server_db.updateClientLastReq_unsafe(origin_ip, pkt.seqn);
    
    // Atualiza resumo
    server_db.updateBankSummary_unsafe();

    string msg_log = "client " + origin_ip + 
                     " id_req " + to_string(pkt.seqn) +
                     " dest " + dest_ip + 
                     " value " + to_string(pkt.rep.value);
    
    // Isso vai fazer o ServerInterface imprimir a linha acima E o resumo (num_transactions, balance...)
    server_interface.notifyUpdate(msg_log);

    // Envia ACK
    Packet ack;
    memset(&ack, 0, sizeof(Packet));
    ack.type = PKT_REPLICATION_ACK;
    ack.seqn = pkt.seqn; 
    sendto(sockfd, &ack, sizeof(Packet), 0, (struct sockaddr*)&sender_addr, sizeof(sender_addr));
}