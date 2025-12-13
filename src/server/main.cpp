#include "server/discovery.h"
#include "server/database.h"
#include "server/processing.h"
#include "server/interface.h"
#include "server/election.h"
#include "server/replication.h"
#include "common/utils.h"
#include "common/protocol.h"
#include <stdexcept>
#include <unistd.h>

int setupServerSocket(int port)
{
    // Cria um socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        log_message("ERROR opening server socket");
        throw runtime_error("Failed to open socket.");
    }

    // Permite reutilizar a porta
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    // Configura o endereço do servidor
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // Faz bind do socket à porta especificada
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        log_message("ERROR on binding server socket");
        close(sockfd);
        throw runtime_error("Failed to bind socket.");
    }
    return sockfd;
}

void onLeaderChange(uint32_t new_leader_id, bool i_am_leader)
{
    if (i_am_leader)
    {
        log_message("=== I AM NOW THE PRIMARY (LEADER) ===");
        replication_manager.setLeader(true);
    }
    else
    {
        log_message(("=== New leader elected: ID " + to_string(new_leader_id) + " ===").c_str());
        replication_manager.setLeader(false);
    }
}

void handlePacket(const Packet &packet,
                  const struct sockaddr_in &client_addr,
                  socklen_t clilen,
                  int sockfd,
                  ServerDiscovery &discovery_handler,
                  ServerProcessing &processing_handler)
{

    // Criamos cópias dos dados de I/O para garantir que a thread não use dados antigos.
    // pois o buffer 'received_packet' será sobrescrito rapidamente pelo runServerLoop.
    Packet packet_copy = packet;
    struct sockaddr_in client_addr_copy = client_addr;

    // DESCOBERTA DE SERVIDORES
    if (packet.type == PKT_SERVER_DISCOVER || packet.type == PKT_SERVER_DISCOVER_ACK)
    {

        int remote_id = packet.server_discovery.id;
        int remote_port = packet.server_discovery.replica_port;

        // Pega o IP de quem mandou
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        string remote_ip(ip_str);

        // Se for mensagem do próprio servidor (broadcast voltou), ignora
        if (election_manager.getMyId() == remote_id)
            return;

        // Adiciona aos gerenciadores
        election_manager.addReplica(remote_id, remote_ip, remote_port);
        replication_manager.addReplica(remote_id, remote_ip, remote_port);

        // Se for um pedido de descoberta (novo servidor), responde com ACK
        if (packet.type == PKT_SERVER_DISCOVER)
        {
            Packet ack;
            memset(&ack, 0, sizeof(Packet));
            ack.type = PKT_SERVER_DISCOVER_ACK;
            ack.server_discovery.id = election_manager.getMyId();
            ack.server_discovery.replica_port = remote_port;

            sendto(sockfd, &ack, sizeof(Packet), 0, (struct sockaddr *)&client_addr, clilen);

            // log_message(("Discovered new server ID " + to_string(remote_id) + ". Sent ACK.").c_str());
        }
        return;
    }

    // MENSAGENS DE ELEIÇÃO (BULLY)
    if (packet.type >= PKT_ELECTION && packet.type <= PKT_HEARTBEAT_ACK)
    {
        switch (packet.type)
        {
        case PKT_ELECTION:
            election_manager.handleElectionMessage(packet, client_addr);
            break;
        case PKT_ELECTION_OK:
            election_manager.handleOkMessage(packet, client_addr);
            break;
        case PKT_COORDINATOR:
            election_manager.handleCoordinatorMessage(packet, client_addr);
            break;
        case PKT_HEARTBEAT:
            election_manager.handleHeartbeatMessage(packet, client_addr);
            break;
        case PKT_HEARTBEAT_ACK:
            election_manager.handleHeartbeatAck(packet, client_addr);
            break;
        default:
            log_message("Unknown election message type.");
            break;
        }
        return;
    }

    // MENSAGENS DE CLIENTE
    switch (packet.type)
    {
    case PKT_DISCOVER:
        // Descoberta é rápida, pode ser tratada na thread principal (só registro e envio de ACK)
        // Apenas o Líder responde a descoberta de clientes.
        if (election_manager.isLeader())
        {
            discovery_handler.handleDiscovery(packet, client_addr, clilen, sockfd);
        }
        else
        {
            log_message("Received PKT_DISCOVER but I'm not the leader. Ignoring.");
        }
        break;

    case PKT_REQUEST:
        // Apenas o líder processa requisições de cliente
        if (election_manager.isLeader())
        {
            // Processar transações em nova thread (uma thread por requisição)
            thread([packet_copy, client_addr_copy, clilen, sockfd, &processing_handler]()
                   { processing_handler.handleRequest(packet_copy, client_addr_copy, clilen, sockfd); })
                .detach();
        }
        else
        {
            log_message("Received PKT_REQUEST but I'm not the leader. Ignoring.");
        }
        break;

    case PKT_REPLICATION_REQ:
    case PKT_REP_CLIENT_REQ:
    case PKT_REP_QUERY_REQ:
        // Recebimento de replicação (Backup recebendo do Líder)
        // Lançamos em thread para manter o padrão não-bloqueante da main
        thread([packet_copy, client_addr_copy]()
               { replication_manager.handleReplicationMessage(packet_copy, client_addr_copy); })
            .detach();
        break;

    case PKT_REPLICATION_ACK:
    case PKT_REP_CLIENT_ACK:
    case PKT_REP_QUERY_ACK:
        // ACK de replicação recebido pelo Líder.
        break;

    default:
        log_message("Received packet with unknown type. Ignoring.");
        break;
    }
}

void runServerLoop(int sockfd, ServerDiscovery &discovery_handler, ServerProcessing &processing_handler)
{
    Packet received_packet;
    struct sockaddr_in client_addr;
    socklen_t clilen = sizeof(client_addr);

    while (true)
    {
        memset(&received_packet, 0, sizeof(Packet));

        // Recebe pacote de qualquer cliente (ou outro servidor)
        ssize_t n = recvfrom(sockfd, (char *)&received_packet, sizeof(Packet), 0,
                             (struct sockaddr *)&client_addr, &clilen);

        if (n < 0)
        {
            log_message("ERROR on recvfrom");
            continue;
        }

        // Delega o processamento baseado no tipo do pacote
        handlePacket(received_packet, client_addr, clilen, sockfd,
                     discovery_handler, processing_handler);
    }
}

// O servidor deve ser iniciado com: ./servidor <CLIENT_PORT>
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <CLIENT_PORT> [REPLICA_PORT]" << endl;
        cerr << "  CLIENT_PORT: Port for client connections" << endl;
        cerr << "  REPLICA_PORT: Port for replica communication (default: CLIENT_PORT+1000)" << endl;
        cerr << "" << endl;
        cerr << "Note: Server ID will be automatically derived from the last byte of the IP address." << endl;
        return 1;
    }

    int client_port;
    int replica_port;
    int server_id;

    try
    {
        client_port = stoi(argv[1]);
        replica_port = (argc >= 3) ? stoi(argv[2]) : (client_port + 1000);
    }
    catch (const exception &e)
    {
        cerr << "Invalid arguments: " << e.what() << endl;
        return 1;
    }

    try
    {
        // Obtém o IP local do servidor
        string my_ip = getMyIP();
        if (my_ip.empty()) {
            cerr << "ERROR: Could not determine local IP address." << endl;
            return 1;
        }
        
        // Deriva o ID do último byte do IP
        server_id = getIdFromIP(my_ip);
        if (server_id < 0) {
            cerr << "ERROR: Could not extract server ID from IP: " << my_ip << endl;
            return 1;
        }
        
        log_message("Starting Server");
        log_message(("My IP: " + my_ip).c_str());
        log_message(("Server ID (from IP): " + to_string(server_id)).c_str());
        log_message(("Client Port: " + to_string(client_port)).c_str());
        log_message(("Replica Port: " + to_string(replica_port)).c_str());

        // Configura sockets
        int client_sockfd = setupServerSocket(client_port);
        int replica_sockfd = setupServerSocket(replica_port);

        election_manager.init(replica_sockfd, server_id, false); // Todos iniciam como follower
        election_manager.setLeaderChangeCallback(onLeaderChange);

        // Inicializa replication_manager (todos iniciam como NOT leader)
        replication_manager.init(replica_sockfd, server_id, false);

        // INICIA MÓDULOS
        server_interface.start();

        // Handlers
        ServerDiscovery discovery_handler;
        ServerProcessing processing_handler;

        // Cliente falso para testes (estado inicial comum)
        const string FAKE_CLIENT_IP = "10.0.0.2";
        if (server_db.addClient(FAKE_CLIENT_IP))
        {
            server_db.updateBankSummary();
            log_message(("Added fake client " + FAKE_CLIENT_IP).c_str());
        }

        // Crie uma thread separada para lidar com mensagens entre servidores (Eleição/Replicação)
        thread replicationThread([replica_sockfd, &discovery_handler, &processing_handler]()
                                 { runServerLoop(replica_sockfd, discovery_handler, processing_handler); });
        replicationThread.detach();

        // Espera a thread subir
        this_thread::sleep_for(chrono::milliseconds(100));

        // Chamada limpa e semântica
        discovery_handler.sendServerBroadcast(replica_sockfd, server_id, replica_port);

        election_manager.start(); // Aqui o Bully determina quem é líder!

        // A thread principal fica no loop ouvindo clientes
        runServerLoop(client_sockfd, discovery_handler, processing_handler);

        election_manager.stop();
        server_interface.stop();
        close(client_sockfd);
        close(replica_sockfd);
    }
    catch (const exception &e)
    {
        cerr << "Fatal error: " << e.what() << endl;
    }

    return 0;
}