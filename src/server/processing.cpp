#include "server/processing.h"
#include "server/database.h"
#include "server/interface.h"
#include "common/protocol.h" 
#include "common/utils.h"    
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <cstring> // Para memset

using namespace std;
extern ServerDatabase server_db;
extern ServerInterface server_interface; 

// --- FUNÇÃO AUXILIAR DE ENVIO DE ACK ---
// Integrada para evitar duplicação de código no envio

void sendResponseAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen, 
                     uint32_t seqn_to_send, uint32_t balance, const string& origin_ip,  uint32_t dest_addr, uint32_t value, bool is_query, bool is_dup_oor) {
    
    Packet ack_packet;
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

    // --- PREPARAÇÃO DOS ENDEREÇOS E VARIÁVEIS ---
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
    if (received_seqn <= last_processed_seqn || received_seqn > last_processed_seqn + 1) {
        // CENA 1 & 2: PACOTE DUPLICADO/ATRASADO (<= last_req) OU FORA DE ORDEM (> last_req + 1)
        buffered_ack = server_db.getClientLastAck(origin_ip_str);
        
        // Verifica se o buffer está válido e aponta para o último processado
        if (buffered_ack.seqn == last_processed_seqn) {
             final_balance = buffered_ack.ack.new_balance;
        } else {
             // Fallback se o buffer estiver vazio/inválido (recalcula o saldo para o último ACK)
             double balance_double = server_db.getClientBalance(origin_ip_str);
             final_balance = static_cast<uint32_t>(balance_double >= 0 ? balance_double : 0);
        }

        // Envio do ACK: Usa o last_processed_seqn como ID de resposta
        sendResponseAck(sockfd, client_addr, clilen, last_processed_seqn, final_balance, 
                            origin_ip_str, buffered_ack.ack.dest_addr, buffered_ack.ack.value, is_query, true);
        return;
    }


    // --- 2. EXECUÇÃO (received_seqn == last_processed_seqn + 1) ---
    if (is_query) {
        
        // CONSULTA DE SALDO (Não é transação)
        double balance_double = server_db.getClientBalance(origin_ip_str);
        
        if (balance_double >= 0) {
            final_balance = static_cast<uint32_t>(balance_double);
            
            server_db.updateClientLastReq(origin_ip_str, received_seqn);
            
            // Cria e bufferiza ACK
            Packet query_ack;
            query_ack.type = PKT_REQUEST_ACK;
            query_ack.seqn = received_seqn;
            query_ack.ack.new_balance = final_balance;
            query_ack.ack.dest_addr = packet.req.dest_addr;
            query_ack.ack.value = packet.req.value;
            server_db.updateClientLastAck_unsafe(origin_ip_str, query_ack);
        }
    } else {
        // Transação Real
        bool success = server_db.makeTransaction(origin_ip_str, dest_ip_str_cpp, packet);

        // Recupera o saldo final para o ACK.
        double balance_double = server_db.getClientBalance(origin_ip_str);
        final_balance = static_cast<uint32_t>(balance_double >= 0 ? balance_double : 0);

        if (!success) {
            log_message("Transação recusada por saldo insuficiente ou erro de validação.");
        }
    }
    
    // --- 3. ENVIO DO ACK DE SUCESSO/EXECUÇÃO ---
    sendResponseAck(sockfd, client_addr, clilen, received_seqn, final_balance, 
                            origin_ip_str, packet.req.dest_addr, packet.req.value, is_query, false);

    // --- 4. NOTIFICA A INTERFACE COM A MENSAGEM DE LOG ---
    string msg_log = "client " + origin_ip_str + 
                     " id_req " + to_string(packet.seqn) +
                     " dest " + dest_ip_str_cpp + 
                     " value " + to_string(packet.req.value);
    server_interface.notifyUpdate(msg_log);
}