#include "common/utils.h"

/* Funções utilitárias (timestamp, formatação de logs) */

string get_timestamp_str() {
    using namespace chrono;
    auto tp = system_clock::now();
    time_t t = system_clock::to_time_t(tp);
    tm tm{};
    localtime_r(&t, &tm); // Thread-safe
    
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

void log_message(const char *msg) {
    //cout << get_timestamp_str() << " " << msg << endl;
    return;
}
void log_message_core(const char *msg) {
    cout << get_timestamp_str() << " " << msg << endl;
    return;
}

uint32_t ipToUint32(const std::string& ip_str) {
    struct in_addr addr;
    
    if (inet_pton(AF_INET, ip_str.c_str(), &addr) != 1) {
        log_message("ERROR: Invalid IP format in ipToUint32.");
        return 0;
    }
    
    return addr.s_addr; 
}

string uint32ToIp(uint32_t ip_int) {
    
    struct in_addr addr;
    addr.s_addr = ip_int;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);

    return string(ip_str); 
}


int getIdFromIP(const string& ip_str) {
    struct in_addr addr;
    
    if (inet_pton(AF_INET, ip_str.c_str(), &addr) != 1) {
        log_message("ERROR: Invalid IP format in getIdFromIP.");
        return -1;
    }
    
    // Extrai o último byte do endereço IP
    // addr.s_addr está em network byte order (big endian)
    // Convertemos para host byte order e pegamos os últimos 8 bits
    uint32_t ip_host = ntohl(addr.s_addr);
    return ip_host & 0xFF; // Último byte
}

// Função para obter o IP local da interface de rede
string getMyIP() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_message("ERROR: Failed to create socket in getMyIP.");
        return "";
    }
    
    // Conecta a um endereço externo (não envia dados, apenas para descobrir a interface)
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8"); // DNS do Google
    serv.sin_port = htons(53);
    
    if (connect(sock, (const struct sockaddr*)&serv, sizeof(serv)) < 0) {
        close(sock);
        log_message("ERROR: Failed to connect in getMyIP.");
        return "";
    }
    
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if (getsockname(sock, (struct sockaddr*)&name, &namelen) < 0) {
        close(sock);
        log_message("ERROR: Failed to get socket name in getMyIP.");
        return "";
    }
    
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);
    
    close(sock);
    return string(buffer);
}