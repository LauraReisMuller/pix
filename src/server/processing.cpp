#include "server/processing.h"
#include "server/database.h"
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

// --- FUNÇÃO AUXILIAR DE ENVIO DE ACK ---
// Integrada para evitar duplicação de código no envio

void sendResponseAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen, 
                     uint32_t seqn_to_send, uint32_t balance, const string& origin_ip, bool is_query, bool is_dup_oor) {
    
    Packet ack_packet;
    ack_packet.type = PKT_REQUEST_ACK;
    ack_packet.seqn = seqn_to_send; 
    ack_packet.ack.new_balance = balance; 

    ssize_t sent_bytes = sendto(sockfd, &ack_packet, sizeof(Packet), 0,
                                (const struct sockaddr*)&client_addr, clilen);

    // --- LOG FINAL (Integrado) ---
    string status_log_part;
    if (is_dup_oor) {
        status_log_part = " (DUP/OOR - ACK ID " + to_string(seqn_to_send) + ")";
    } else if (is_query) {
        status_log_part = " (Consulta)";
    } else {
        status_log_part = (balance > 0 ? " (OK)" : " (Falha)");
    }

    string msg_final = "ACK sent to " + origin_ip + 
                       " | ID: " + to_string(seqn_to_send) +
                       " | New Balance: " + to_string(balance) + status_log_part;
                       
    if (sent_bytes < 0) {
        log_message(("ERROR sending ACK for ID " + to_string(seqn_to_send) + " to client.").c_str());
    } else {
        log_message(msg_final.c_str());
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
    
    // Log da requisição recebida
    string msg_log = "Request received from " + origin_ip_str + 
                     " | ID: " + to_string(packet.seqn) +
                     " | Dest: " + dest_ip_str_cpp + 
                     " | Value: " + to_string(packet.req.value);
    log_message(msg_log.c_str());

    uint32_t final_balance = 0; 
    bool is_query = (packet.req.value == 0);
    
    // --- 1. VERIFICAÇÃO DE DUPLICIDADE/SEQUÊNCIA (CRÍTICO) ---
    uint32_t last_processed_seqn = server_db.getClientLastReq(origin_ip_str);
    uint32_t received_seqn = packet.seqn;
    
    if (received_seqn <= last_processed_seqn || received_seqn > last_processed_seqn + 1) {
        // CENA 1 & 2: PACOTE DUPLICADO/ATRASADO (<= last_req) OU FORA DE ORDEM (> last_req + 1)
        Packet buffered_ack = server_db.getClientLastAck(origin_ip_str);
        
        // Verifica se o buffer está válido e aponta para o último processado
        if (buffered_ack.seqn == last_processed_seqn) {
             final_balance = buffered_ack.ack.new_balance;
        } else {
             // Fallback se o buffer estiver vazio/inválido (recalcula o saldo para o último ACK)
             double balance_double = server_db.getClientBalance(origin_ip_str);
             final_balance = static_cast<uint32_t>(balance_double >= 0 ? balance_double : 0);
        }

        // Envio do ACK: Usa o last_processed_seqn como ID de resposta
        sendResponseAck(sockfd, client_addr, clilen, last_processed_seqn, final_balance, origin_ip_str, is_query, true);
        return; // CRUCIAL: Sai da função sem processar.
    }


    // --- 2. EXECUÇÃO (received_seqn == last_processed_seqn + 1) ---
    
    if (is_query) {
        
        // CONSULTA DE SALDO (Não é transação)
        double balance_double = server_db.getClientBalance(origin_ip_str);
        
        if (balance_double >= 0) {
        final_balance = static_cast<uint32_t>(balance_double);
        log_message(("Consulta de saldo OK. Saldo: " + to_string(final_balance)).c_str());

        // ATUALIZAÇÃO NECESSÁRIA: Sinaliza que a requisição de consulta foi processada.
        server_db.updateClientLastReq_unsafe(origin_ip_str, received_seqn); // CHAMA A VERSÃO _UNSAFE
        }
    } else {
        // TRANSAÇÃO REAL (Chama o COMMIT ATÔMICO)
        
        bool success = server_db.makeTransaction(origin_ip_str, dest_ip_str_cpp, packet);

        // Recupera o saldo final para o ACK.
        double balance_double = server_db.getClientBalance(origin_ip_str);
        final_balance = static_cast<uint32_t>(balance_double >= 0 ? balance_double : 0);

        if (success) {
            log_message("Transação concluída com sucesso.");
        } else {
            log_message("Transação recusada por saldo insuficiente ou erro de validação.");
        }
    }
    
    // --- 3. ENVIO DO ACK DE SUCESSO/EXECUÇÃO ---
    // Envia o ACK confirmando que o pacote recebido (REQ #N) foi processado.
    sendResponseAck(sockfd, client_addr, clilen, received_seqn, final_balance, origin_ip_str, is_query, false);
}