#include "server/discovery.h"
#include "server/database.h"
#include "server/processing.h"
#include "common/utils.h"
#include "common/protocol.h"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

using namespace std;

//apenas para o Cliente fake
extern ServerDatabase server_db;


/**
 * @brief Cria e configura um socket UDP para o servidor.
 * @param port Porta na qual o servidor irá escutar
 * @return File descriptor do socket criado
 * @throws runtime_error Se falhar ao criar ou fazer bind do socket
 */

int setupServerSocket(int port) {
    // Cria um socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_message("ERROR opening server socket");
        throw runtime_error("Failed to open socket.");
    }

    // Permite reutilizar a porta (útil para reiniciar o servidor rapidamente)
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    // Configura o endereço do servidor
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;  // Escuta em todas as interfaces
    serv_addr.sin_port = htons(port);

    // Faz bind do socket à porta especificada
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        log_message("ERROR on binding server socket");
        close(sockfd);
        throw runtime_error("Failed to bind socket.");
    }

    string msg = "Server listening on port " + to_string(port);
    log_message(msg.c_str());

    return sockfd;
}

/**
 * @brief Processa um pacote recebido, delegando para o handler apropriado.
 * @param packet Pacote recebido
 * @param client_addr Endereço do cliente que enviou o pacote
 * @param clilen Tamanho da estrutura sockaddr_in
 * @param sockfd Socket para enviar respostas
 * @param discovery_handler Handler de descoberta
 * @param processing_handler Handler de processamento de requisições
 */
void handlePacket(const Packet& packet, 
                  const struct sockaddr_in& client_addr, 
                  socklen_t clilen, 
                  int sockfd,
                  ServerDiscovery& discovery_handler, 
                  ServerProcessing& processing_handler) {
    
    switch (packet.type) {
        case PKT_DISCOVER:
            discovery_handler.handleDiscovery(packet, client_addr, clilen, sockfd);
            break;
        
        case PKT_REQUEST:
            processing_handler.handleRequest(packet, client_addr, clilen, sockfd);
            break;
        
        default:
            log_message("Received packet with unknown type. Ignoring.");
            break;
    }
}

/**
 * @brief Loop principal do servidor que recebe e processa pacotes.
 * @param sockfd Socket do servidor
 * @param discovery_handler Handler de descoberta
 * @param processing_handler Handler de processamento de requisições
 */
void runServerLoop(int sockfd, ServerDiscovery& discovery_handler, ServerProcessing& processing_handler) {
    Packet received_packet;
    struct sockaddr_in client_addr;
    socklen_t clilen = sizeof(client_addr);

    while (true) {
        memset(&received_packet, 0, sizeof(Packet));
        
        // Recebe pacote de qualquer cliente
        ssize_t n = recvfrom(sockfd, (char*)&received_packet, sizeof(Packet), 0,
                             (struct sockaddr*)&client_addr, &clilen);
        
        if (n < 0) {
            log_message("ERROR on recvfrom");
            continue;
        }

        // Delega o processamento baseado no tipo do pacote
        handlePacket(received_packet, client_addr, clilen, sockfd, 
                    discovery_handler, processing_handler);
    }
}

// O servidor deve ser iniciado com a porta UDP como parâmetro: ./servidor 4000 
int main(int argc, char* argv[]) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <PORT>" << endl;
        return 1;
    }
    
    int port;
    try {
        port = stoi(argv[1]);
    } catch (const invalid_argument& e) {
        cerr << "Invalid port number." << endl;
        return 1;
    } catch (const out_of_range& e) {
        cerr << "Port number out of range." << endl;
        return 1;
    }

    try {
        // Configura o socket do servidor
        int sockfd = setupServerSocket(port);

        // Cria instâncias dos handlers
        ServerDiscovery discovery_handler;
        ServerProcessing processing_handler;

        /*CLIENTE FAKE PARA TESTE*/
        const string FAKE_CLIENT_IP = "10.0.0.2";
        
        // Garanta que o cliente fake seja adicionado.
        if (server_db.addClient(FAKE_CLIENT_IP)) {
            log_message(("SUCCESS: Added fake client " + FAKE_CLIENT_IP + " for testing.").c_str());
        } else {
            log_message("WARNING: Fake client already existed or failed to add.");
        }

        // Inicia o loop principal do servidor
        runServerLoop(sockfd, discovery_handler, processing_handler);

        close(sockfd);

    } catch (const exception& e) {
        cerr << "Server Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}