#include "server/processing.h"
#include "server/database.h"
#include <unistd.h>
#include <stdexcept>

void ServerProcessing::handleRequest(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
    // Valida se o pacote recebido é uma requisição
    if (packet.type != PKT_REQUEST) {
        log_message("Received non-request packet. Ignoring.");
        return;
    }

    // Converte o endereço do cliente para string (para log)
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    char dest_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &packet.req.dest_addr, dest_ip, INET_ADDRSTRLEN);

    // Log da requisição recebida
    string msg = "Request received from " + string(client_ip) + 
                 " | ID: " + to_string(packet.seqn) +
                 " | Dest: " + string(dest_ip) + 
                 " | Value: " + to_string(packet.req.value);
    log_message(msg.c_str());

    server_db.makeTransaction(client_ip, dest_ip, packet);

    // Prepara o pacote de ACK
    Packet ack_packet;
    ack_packet.type = PKT_REQUEST_ACK;
    ack_packet.seqn = packet.seqn;  // Mesmo número de sequência da requisição
    ack_packet.ack.seqn = packet.seqn;
    ack_packet.ack.new_balance = server_db.getClient(client_ip)->balance;  // Saldo fictício para teste

    // Envia o ACK de volta para o cliente usando o socket externo
    ssize_t sent_bytes = sendto(sockfd, &ack_packet, sizeof(Packet), 0,
                                (const struct sockaddr*)&client_addr, clilen);

    if (sent_bytes < 0) {
        log_message("ERROR sending ACK to client.");
        return;
    }

    msg = "ACK sent to " + string(client_ip) + " | ID: " + to_string(packet.seqn);
    log_message(msg.c_str());
}