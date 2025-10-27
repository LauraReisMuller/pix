/**
 * Responsável por processar pedidos PKT_REQUEST recebidos dos clientes.
 *
 * Funcionalidades principais:
 *  - detecção de pedidos duplicados / fora de ordem (usando last_req por cliente)
 *  - resposta imediata com ACK contendo o novo saldo
 *  - execução de transacções através de `ServerDatabase::makeTransaction`
 */


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
#include <cstring>

using namespace std;
extern ServerDatabase server_db;
extern ServerInterface server_interface; 

void sendResponseAck(int sockfd, const struct sockaddr_in& client_addr, socklen_t clilen, 
                     uint32_t seqn_to_send, uint32_t balance, const string& origin_ip,  uint32_t dest_addr, uint32_t value, bool is_query, bool is_dup_oor) {
    cout<<"Em processing..."<<to_string(value)<<endl;
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

    // DEBUG: Verifica o valor recebido
    //log_message(("DEBUG: packet.req.value RAW = " + to_string(packet.req.value)).c_str());
    //log_message(("DEBUG: packet.req.value ESPERADO = " + to_string(packet.req.value)).c_str());

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
        
        if (buffered_ack.seqn == last_processed_seqn) {
             final_balance = buffered_ack.ack.new_balance;
        } else {
             uint32_t balance_uint32_t = server_db.getClientBalance(origin_ip_str);
             final_balance = static_cast<uint32_t>(balance_uint32_t >= 0 ? balance_uint32_t : 0);
        }

        sendResponseAck(sockfd, client_addr, clilen, last_processed_seqn, final_balance, 
                            origin_ip_str, buffered_ack.ack.dest_addr, buffered_ack.ack.value, is_query, true);

        server_interface.notifyUpdate(dup_msg);

        return;
    }

    if (is_query) {
        uint32_t balance_uint32_t = server_db.getClientBalance(origin_ip_str);
        
        if (balance_uint32_t >= 0) {
            final_balance = static_cast<uint32_t>(balance_uint32_t);
            
            server_db.updateClientLastReq(origin_ip_str, received_seqn);
            Packet query_ack;
            query_ack.type = PKT_REQUEST_ACK;
            query_ack.seqn = received_seqn;
            query_ack.ack.new_balance = final_balance;
            query_ack.ack.dest_addr = packet.req.dest_addr;
            query_ack.ack.value = packet.req.value;
            server_db.updateClientLastAck(origin_ip_str, query_ack);
        }
    } else {
        bool success = server_db.makeTransaction(origin_ip_str, dest_ip_str_cpp, packet);
        uint32_t balance_uint32_t = server_db.getClientBalance(origin_ip_str);
        final_balance = static_cast<uint32_t>(balance_uint32_t >= 0 ? balance_uint32_t : 0);

        if (!success) {
            log_message("Transação recusada por saldo insuficiente ou erro de validação.");
        }
    }
    
    sendResponseAck(sockfd, client_addr, clilen, received_seqn, final_balance, 
                            origin_ip_str, packet.req.dest_addr, packet.req.value, is_query, false);

    string msg_log = "client " + origin_ip_str + 
                     " id_req " + to_string(packet.seqn) +
                     " dest " + dest_ip_str_cpp + 
                     " value " + to_string(packet.req.value);
    server_interface.notifyUpdate(msg_log);
}