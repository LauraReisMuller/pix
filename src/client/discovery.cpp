#include "client/discovery.h"
#include "common/protocol.h"
#include "common/utils.h"
#include <ifaddrs.h>
#include <net/if.h>

#define DISCOVERY_TIMEOUT_SEC 1
#define MAX_DISCOVERY_ATTEMPTS 50

string getBroadcastAddress() {
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    string broadcastAddr = "255.255.255.255"; // Valor padrão (fallback)

    // Recupera a lista de interfaces de rede da máquina/container
    if (getifaddrs(&interfaces) == 0) {
        temp_addr = interfaces;
        while (temp_addr != NULL) {
            // Verifica se é IPv4 (AF_INET)
            if (temp_addr->ifa_addr && temp_addr->ifa_addr->sa_family == AF_INET) {
                // Ignora interface de Loopback (127.0.0.1)
                if (!(temp_addr->ifa_flags & IFF_LOOPBACK)) {
                    // Verifica se suporta Broadcast
                    if (temp_addr->ifa_flags & IFF_BROADCAST) {
                        // Converte o endereço binário de broadcast para string
                        void* ptr = &((struct sockaddr_in*)temp_addr->ifa_broadaddr)->sin_addr;
                        char buffer[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, ptr, buffer, INET_ADDRSTRLEN);
                        
                        broadcastAddr = string(buffer);
                        // Geralmente a primeira interface válida (eth0) é a que queremos
                        break; 
                    }
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }
    freeifaddrs(interfaces);
    return broadcastAddr;
}

ClientDiscovery::ClientDiscovery(int port) : _port(port), _sockfd(-1) {

    // inicializa a estrutura de endereço do servidor para o broadcast
    memset(&_serv_addr, 0, sizeof(_serv_addr));
    _serv_addr.sin_family = AF_INET;
    _serv_addr.sin_port = htons(_port);

    // [MODIFICAÇÃO] Busca o IP de Broadcast real da rede
    string brd_ip = getBroadcastAddress();
    _serv_addr.sin_addr.s_addr = inet_addr(brd_ip.c_str());
    
    // Log para você saber o que está acontecendo
    cout << "[DEBUG] Broadcast configurado para: " << brd_ip << endl;
}


//Cria o socket UDP e habilita a opção broadcast.
void ClientDiscovery::setupSocket() {
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0) {
        log_message("ERROR opening client socket");
        throw runtime_error("Failed to open socket.");
    }
    
    // Habilita a opção SO_BROADCAST no socket
    int enable = 1;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
        log_message("ERROR setting SO_BROADCAST option");
        close(_sockfd);
        throw runtime_error("Failed to set SO_BROADCAST.");
    }
}

//Espera pela resposta (ACK) do servidor com um timeout.
bool ClientDiscovery::waitForResponse(sockaddr_in& server_info, socklen_t& len) {
    fd_set read_fds;
    struct timeval tv;
    int retval;

    // Configura o set de descritores de arquivo
    FD_ZERO(&read_fds);
    FD_SET(_sockfd, &read_fds);

    // Configura o timeout
    tv.tv_sec = DISCOVERY_TIMEOUT_SEC;
    tv.tv_usec = 0;

    // Monitora o socket para dados de entrada (bloqueia até que haja dados ou timeout)
    retval = select(_sockfd + 1, &read_fds, NULL, NULL, &tv);

    if (retval == -1) {
        // Erro na chamada select
        log_message("ERROR in select() during discovery");
        return false;
    } else if (retval == 0) {
        // Timeout
        log_message("Discovery timeout. Retrying...");
        return false;
    } else {
        // Dados recebidos (sockfd está no read_fds set)
        Packet response_packet;
        
        // recvfrom receberá a resposta unicast do servidor
        ssize_t n = recvfrom(_sockfd, (char*)&response_packet, sizeof(Packet), 0, 
                             (struct sockaddr*)&server_info, &len);
        
        //Verifica se o pacote recebido é realmente um ACK de descoberta
        if (response_packet.type != PKT_DISCOVER_ACK) {
            log_message("Received unexpected packet type during discovery. Ignoring.");
            return false;
        }
        
        if (n < 0) {
            log_message("ERROR on recvfrom during discovery");
            return false;
        }
        return true;
    }
}

//Realiza o processo de Descoberta.
string ClientDiscovery::discoverServer() {
    
    try {
        setupSocket();
    } catch (const runtime_error& e) {
        return ""; 
    }

    // Prepara o pacote de DESCOBERTA
    Packet discovery_packet;
    discovery_packet.type = PKT_DISCOVER;
    discovery_packet.seqn = 0; // O ID não é relevante para o Descobrimento

    struct sockaddr_in server_info;
    socklen_t len = sizeof(server_info);

    for (int i = 0; i < MAX_DISCOVERY_ATTEMPTS; ++i) {
        
        // 1. Envia o pacote de Descoberta em broadcast
        ssize_t n = sendto(_sockfd, (const char*)&discovery_packet, sizeof(Packet), 0, 
                           (const struct sockaddr*)&_serv_addr, sizeof(_serv_addr));
        
        if (n < 0) {
            log_message("ERROR sending discovery broadcast");
            continue; // Tenta novamente
        }
        
        // 2. Aguarda a resposta (ACK) unicast do servidor com timeout
        if (waitForResponse(server_info, len)) {
    
            // Converte o IP para string
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &server_info.sin_addr, ip_str, INET_ADDRSTRLEN);
            
            // Fecha o socket e retorna o IP
            close(_sockfd);
            return string(ip_str);
        }
    }

    // 3. Falha após N tentativas
    log_message("Failed to discover server after multiple attempts.");
    if (_sockfd >= 0) {
        close(_sockfd);
    }
    return "";
}
