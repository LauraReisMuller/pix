#include "server/discovery.h"
#include "server/database.h"
#include "server/processing.h"
#include "server/interface.h"
#include "common/utils.h"
#include "common/protocol.h"
#include "server/replication.h"
#include <stdexcept>
#include <unistd.h>

ReplicationManager replication_manager;

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
    return sockfd;
}


void handlePacket(const Packet& packet, 
                  const struct sockaddr_in& client_addr, 
                  socklen_t clilen, 
                  int sockfd,
                  ServerDiscovery& discovery_handler, 
                  ServerProcessing& processing_handler) {
    
    //Criamos cópias dos dados de I/O para garantir que a thread não use dados antigos.
    //pois o buffer 'received_packet' será sobrescrito rapidamente pelo runServerLoop.
    Packet packet_copy = packet;
    struct sockaddr_in client_addr_copy = client_addr;

    switch (packet.type) {
        case PKT_DISCOVER:

            // Descoberta é rápida, pode ser tratada na thread principal (só registro e envio de ACK)
            // Apenas o Líder responde a descoberta de clientes.
            // Se for backup, ignoramos silenciosamente.
            if (replication_manager.isLeader()) {
                discovery_handler.handleDiscovery(packet, client_addr, clilen, sockfd);
            }
            break;
        
        case PKT_REQUEST:
            //Processar transações em nova thread (uma thread por requisição)
            thread([packet_copy, client_addr_copy, clilen, sockfd, &processing_handler]() {
                processing_handler.handleRequest(packet_copy, client_addr_copy, clilen, sockfd);
            }).detach(); 
            break;

        case PKT_REPLICATION_REQ:
        case PKT_REP_CLIENT_REQ:
            //Recebimento de replicação (Backup recebendo do Líder)
            //Lançamos em thread para manter o padrão não-bloqueante da main
            thread([packet_copy, client_addr_copy]() {
                replication_manager.handleReplicationMessage(packet_copy, client_addr_copy);
            }).detach();
            break;

        case PKT_REPLICATION_ACK:
        case PKT_REP_CLIENT_ACK:
            //ACK de replicação recebido pelo Líder.
            //Geralmente isso é pego pelo recvfrom dentro do loop de espera do 'replicateTransaction'.
            //Se cair aqui, é porque chegou atrasado ou duplicado. Apenas ignoramos.
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
        
        // Recebe pacote de qualquer cliente (ou outro servidor)
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

// O servidor deve ser iniciado com: ./servidor <PORT> <ID> <IS_LEADER>
// Ex Líder:  ./servidor 4000 0 1
// Ex Backup: ./servidor 4001 1 0
int main(int argc, char* argv[]) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <PORT> [ID] [IS_LEADER (1/0)]" << endl;
        return 1;
    }
    
    int port;
    int my_id = 0;       // Default ID
    bool is_leader = true; // Default Leader (para manter compatibilidade se não passar args)

    try {
        port = stoi(argv[1]);
        //Parsing de argumentos extras para Replicação
        if (argc > 2) my_id = stoi(argv[2]);
        if (argc > 3) is_leader = (stoi(argv[3]) == 1);
    } catch (const exception& e) {
        cerr << "Invalid arguments: " << e.what() << endl;
        return 1;
    }

    try {
        // Configura o socket do servidor
        int sockfd = setupServerSocket(port);
        
        //Inicializa o Replication Manager com o socket criado
        replication_manager.init(sockfd, my_id, is_leader);

        // Inicia a thread da interface (não-bloqueante)
        server_interface.start();

        // Cria instâncias dos handlers
        ServerDiscovery discovery_handler;
        ServerProcessing processing_handler;

/*
        // CLIENTE FAKE PARA TESTE
        const string FAKE_CLIENT_IP = "10.0.0.2";
        
        // Garanta que o cliente fake seja adicionado.
        if (server_db.addClient(FAKE_CLIENT_IP)) {
            log_message(("SUCCESS: Added fake client " + FAKE_CLIENT_IP + " for testing.\n").c_str());
        } else {
            log_message("WARNING: Fake client already existed or failed to add.");
        }
*/
        // Inicia o loop principal do servidor
        runServerLoop(sockfd, discovery_handler, processing_handler);

        // Para a interface ao encerrar
        server_interface.stop();
        close(sockfd);

    } catch (const exception& e) {
        cerr << "Server Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}