#include "server/processing.h"
#include "server/database.h"
#include "common/protocol.h" 
#include "common/utils.h"    
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <arpa/inet.h>

using namespace std;
extern ServerDatabase server_db;

void ServerProcessing::handleRequest(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
    //Valida se o pacote recebido é uma requisição
    if (packet.type != PKT_REQUEST) {
        log_message("Received non-request packet. Ignoring.");
        return;
    }

    //Converte o endereço do cliente para string (para log)
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    char dest_ip[INET_ADDRSTRLEN];
    struct in_addr dest_addr;
    dest_addr.s_addr = packet.req.dest_addr;
    inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);


    //Log da requisição recebida
    string msg = "Request received from " + string(client_ip) + 
                 " | ID: " + to_string(packet.seqn) +
                 " | Dest: " + string(dest_ip) + 
                 " | Value: " + to_string(packet.req.value);
    log_message(msg.c_str());

    uint32_t final_balance; 
    bool is_query = (packet.req.value == 0);

    
    if (is_query) {
        //CONSULTA DE SALDO (IP_DESTINO 0)
        
        //Busca o saldo do cliente de ORIGEM
        double balance_double = server_db.getClientBalance(std::string(client_ip));

        if (balance_double >= 0) {
            final_balance = static_cast<uint32_t>(balance_double);
            log_message(("Consulta de saldo OK. Saldo: " + to_string(final_balance)).c_str());
        } else {
            //Cliente não encontrado ou erro. Retorna 0 no saldo.
            final_balance = 0;
            log_message("ERRO: Cliente de origem não encontrado para consulta de saldo.");
        }
        
    } else {
        //TRANSAÇÃO REAL 
        
        // Adaptação da sua chamada original (presume que makeTransaction faz as validações internas)
        bool success = server_db.makeTransaction(string(client_ip), string(dest_ip), packet);

        // Recupera o saldo final para enviar no ACK (independente de sucesso ou falha)
        double balance_double = server_db.getClientBalance(std::string(client_ip));
        final_balance = static_cast<uint32_t>(balance_double >= 0 ? balance_double : 0);

        if (success) {
            log_message("Transação concluída com sucesso.");
        } else {
            log_message("Transação recusada por saldo insuficiente ou erro de validação.");
        }
    }
    
    
    Packet ack_packet;
    ack_packet.type = PKT_REQUEST_ACK;
    ack_packet.seqn = packet.seqn; 
    
    // O saldo retornado no ACK é o saldo atualizado (ou consultado) do cliente de origem.
    ack_packet.ack.new_balance = final_balance; 

    // Envia o ACK de volta para o cliente
    ssize_t sent_bytes = sendto(sockfd, &ack_packet, sizeof(Packet), 0,
                                (const struct sockaddr*)&client_addr, clilen);

    if (sent_bytes < 0) {
        log_message(("ERROR sending ACK for ID " + to_string(packet.seqn) + " to client.").c_str());
        return;
    }

    // Log final (adaptado)
    string msg_final = "ACK sent to " + string(client_ip) + 
                       " | ID: " + to_string(packet.seqn) +
                       " | New Balance: " + to_string(final_balance) + 
                       (is_query ? " (Consulta)" : (final_balance > 0 ? "" : " (Falha)"));
                       
    log_message(msg_final.c_str());
}