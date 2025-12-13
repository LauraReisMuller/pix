#include "server/processing.h"

using namespace std;


void sendResponseAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen, 
                     uint32_t seqn_to_send, uint32_t balance, const string& origin_ip,  uint32_t dest_addr, uint32_t value, bool is_query, bool is_dup_oor) {
    Packet ack_packet;
    memset(&ack_packet, 0, sizeof(Packet));
    ack_packet.type = PKT_REQUEST_ACK;
    ack_packet.seqn = seqn_to_send; 
    ack_packet.ack.new_balance = balance; 
    ack_packet.ack.dest_addr = dest_addr;
    ack_packet.ack.value = value;

    ssize_t sent_bytes = sendto(sockfd, &ack_packet, sizeof(Packet), 0,
                                (const struct sockaddr*)&client_addr, clilen);
                       
    if (sent_bytes < 0) {
        log_message(("ERROR sending ACK for ID " + to_string(seqn_to_send) + " to client.").c_str());
    }
}


void ServerProcessing::handleRequest(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
    if (packet.type != PKT_REQUEST) {
        log_message("Received non-request packet. Ignoring.");
        return;
    }
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    string origin_ip_str(client_ip);
    
    char dest_ip_str[INET_ADDRSTRLEN];
    struct in_addr dest_addr;
    dest_addr.s_addr = packet.req.dest_addr;
    inet_ntop(AF_INET, &dest_addr, dest_ip_str, INET_ADDRSTRLEN);
    string dest_ip_str_cpp(dest_ip_str);
    
    uint32_t final_balance = 0; 
    bool is_query = (packet.req.value == 0);
    
    // --- 1. VERIFICAÇÃO DE DUPLICIDADE/SEQUÊNCIA (CRÍTICO) ---
    uint32_t last_processed_seqn = server_db.getClientLastReq(origin_ip_str);
    uint32_t received_seqn = packet.seqn;
    
    Packet buffered_ack;
    bool duplicate_packet = (received_seqn <= last_processed_seqn);
    bool out_of_order_packet = (received_seqn > last_processed_seqn + 1);

    if (duplicate_packet || out_of_order_packet) {
        string log_prefix = duplicate_packet ? " DUP!!" : "";
        string dup_msg = "client " + origin_ip_str + 
                        log_prefix + 
                        " id_req " + to_string(received_seqn) +
                        " dest " + dest_ip_str_cpp + 
                        " value " + to_string(packet.req.value);

        buffered_ack = server_db.getClientLastAck(origin_ip_str);
        
        uint32_t ack_dest_addr = packet.req.dest_addr;
        uint32_t ack_value = packet.req.value;
        
        if (buffered_ack.seqn == last_processed_seqn) {
             final_balance = buffered_ack.ack.new_balance;
        } else {
             uint32_t balance = server_db.getClientBalance(origin_ip_str);
             final_balance = (balance >= 0 ? balance : 0);
        }

        // Envio do ACK: Usa o last_processed_seqn como ID de resposta
        sendResponseAck(sockfd, client_addr, clilen, last_processed_seqn, final_balance, 
                        origin_ip_str, ack_dest_addr, ack_value, is_query, true);

        // Notifica a interface sobre o pacote duplicado/fora de ordem
        server_interface.notifyUpdate(dup_msg);

        return;
    }


    // 2. EXECUÇÃO (received_seqn == last_processed_seqn + 1)
    if (is_query) {
        uint32_t balance = server_db.getClientBalance(origin_ip_str);
        
        if (balance >= 0) {
            final_balance = balance;
            
            server_db.updateClientLastReq(origin_ip_str, received_seqn);

            bool replicated = replication_manager.replicateQuery(
                origin_ip_str, 
                packet.seqn
            );

            if (!replicated) {
                log_message("AVISO: Falha ao replicar QUERY para backups.");
            }
            
            // Cria e bufferiza ACK
            Packet query_ack;
            query_ack.type = PKT_REQUEST_ACK;
            query_ack.seqn = received_seqn;
            query_ack.ack.new_balance = final_balance;
            query_ack.ack.dest_addr = packet.req.dest_addr;
            query_ack.ack.value = packet.req.value;
            server_db.updateClientLastAck(origin_ip_str, query_ack);
        }
    } else {

        //Transação real (com replicação)
        //Se for Backup, ignora silenciosamente (return)
        if (!replication_manager.isLeader()) return;

        // 1. O Líder Executa localmente primeiro!
        // (Sua makeTransaction já valida saldo e cliente, então se falhar, retorna false)
        bool success = server_db.makeTransaction(origin_ip_str, dest_ip_str_cpp, packet);

        if (!success) {
            log_message("Transação recusada localmente (Saldo/Cliente). Não vou replicar.");
            // Manda "NACK" pro cliente
            uint32_t current_bal = server_db.getClientBalance(origin_ip_str);
            sendResponseAck(sockfd, client_addr, clilen, received_seqn, current_bal, 
                            origin_ip_str, packet.req.dest_addr, packet.req.value, false, false);
            return;
        }

        // 2. Coletar o "Estado Atualizado"
        uint32_t bal_orig = server_db.getClientBalance(origin_ip_str);
        uint32_t bal_dest = server_db.getClientBalance(dest_ip_str_cpp);

        // 3. Replicar o estado
        // Criamos uma nova função que aceita os saldos
        bool replicated = replication_manager.replicateState(
            origin_ip_str, dest_ip_str_cpp, 
            packet.req.value, packet.seqn,
            bal_orig, bal_dest
        );

        if (!replicated) {
            log_message("AVISO: Falha ao replicar estado para backups.");
        }
        
        // 4. Responder ao Cliente
        final_balance = bal_orig;
        sendResponseAck(sockfd, client_addr, clilen, received_seqn, final_balance, 
                            origin_ip_str, packet.req.dest_addr, packet.req.value, is_query, false);
    }
    
    sendResponseAck(sockfd, client_addr, clilen, received_seqn, final_balance, 
                            origin_ip_str, packet.req.dest_addr, packet.req.value, is_query, false);

    string msg_log = "client " + origin_ip_str + 
                     " id_req " + to_string(packet.seqn) +
                     " dest " + dest_ip_str_cpp + 
                     " value " + to_string(packet.req.value);
    server_interface.notifyUpdate(msg_log);
}