#include "server/discovery.h"
#include "common/protocol.h"
#include "server/database.h"
#include "common/utils.h"

void ServerDiscovery::handleDiscovery(const Packet& packet, const struct sockaddr_in& client_addr, socklen_t clilen, int sockfd) {
    
    if (packet.type != PKT_DISCOVER) {
        return; 
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    string client_key = string(client_ip);
    
    // Log antes de adicionar
    log_message(("Discovery: Attempting to add client: " + client_key).c_str());
    
    bool added = server_db.addClient(client_key);
    
    // Log do resultado
    if (added) {
        log_message(("Discovery: Client added successfully: " + client_key).c_str());
    } else {
        log_message(("Discovery: Client already exists: " + client_key).c_str());
    }
    
    server_db.updateBankSummary();
    
    // Log após atualizar resumo
    BankSummary summary = server_db.getBankSummary();
    log_message(("Discovery: BankSummary updated - Total Balance: " + 
                to_string(summary.total_balance) + 
                ", Num Transactions: " + to_string(summary.num_transactions)).c_str());
    
    // Criar um pacote ACK/Resposta para Descoberta
    Packet discovery_ack;
    discovery_ack.type = PKT_DISCOVER_ACK;
    discovery_ack.seqn = 0; 
    
    ssize_t n = sendto(sockfd, (const char*)&discovery_ack, sizeof(Packet), 0, 
                       (const struct sockaddr *) &client_addr, clilen);
    
    if (n < 0) {
        log_message("ERROR on sendto discovery ACK");
    }
}