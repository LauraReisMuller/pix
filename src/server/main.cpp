/**
 * Servidor principal: inicializa socket UDP de escuta, arranca a interface do servidor,
 * cria handlers de descoberta/processamento e entra no loop principal (recvfrom).
 *
 * Cada pedido de processamento é despachado para uma thread separada (detach)
 * para não bloquear o loop de recepção.
 */

#include "server/discovery.h"
#include "server/database.h"
#include "server/processing.h"
#include "server/interface.h"
#include "common/utils.h"
#include "common/protocol.h"
#include <stdexcept>
#include <unistd.h>

using namespace std;

int setupServerSocket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        log_message("ERROR opening server socket");
        throw runtime_error("Failed to open socket.");
    }
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        log_message("ERROR on binding server socket");
        close(sockfd);
        throw runtime_error("Failed to bind socket.");
    }
    return sockfd;
}

void handlePacket(const Packet& packet, 
                  const struct sockaddr_in& client_addr, 
                  socklen_t clilen, 
                  int sockfd,
                  ServerDiscovery& discovery_handler, 
                  ServerProcessing& processing_handler) {
    
    Packet packet_copy = packet;
    struct sockaddr_in client_addr_copy = client_addr;

    switch (packet.type) {
        case PKT_DISCOVER:

            discovery_handler.handleDiscovery(packet, client_addr, clilen, sockfd);
            break;
        
        case PKT_REQUEST:
            thread([packet_copy, client_addr_copy, clilen, sockfd, &processing_handler]() {
                processing_handler.handleRequest(packet_copy, client_addr_copy, clilen, sockfd);
            }).detach(); 
            break;
        
        default:
            log_message("Received packet with unknown type. Ignoring.");
            break;
    }
}


void runServerLoop(int sockfd, ServerDiscovery& discovery_handler, ServerProcessing& processing_handler) {
    Packet received_packet;
    struct sockaddr_in client_addr;
    socklen_t clilen = sizeof(client_addr);

    while (true) {
        memset(&received_packet, 0, sizeof(Packet));
        
        ssize_t n = recvfrom(sockfd, (char*)&received_packet, sizeof(Packet), 0,
                             (struct sockaddr*)&client_addr, &clilen);
        
        if (n < 0) {
            log_message("ERROR on recvfrom");
            continue;
        }

        handlePacket(received_packet, client_addr, clilen, sockfd, 
                    discovery_handler, processing_handler);
    }
}

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
        int sockfd = setupServerSocket(port);

        server_interface.start();

        ServerDiscovery discovery_handler;
        ServerProcessing processing_handler;

        const string FAKE_CLIENT_IP = "10.0.0.2";
        
        if (server_db.addClient(FAKE_CLIENT_IP)) {
            log_message(("SUCCESS: Added fake client " + FAKE_CLIENT_IP + " for testing.\n").c_str());
        } else {
            log_message("WARNING: Fake client already existed or failed to add.");
        }

        runServerLoop(sockfd, discovery_handler, processing_handler);
        
        server_interface.stop();
        close(sockfd);

    } catch (const exception& e) {
        cerr << "Server Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}