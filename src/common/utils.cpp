// Funções utilitárias (timestamp, formatação de logs)
#include "common/utils.h"
#include <stdio.h>
#include <time.h>

void print_timestamp() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

void log_message(const char *msg) {
    print_timestamp();
    printf("%s\n", msg);
}

uint32_t ipToUint32(const std::string& ip_str) {

    struct in_addr addr;
    
    if (inet_pton(AF_INET, ip_str.c_str(), &addr) != 1) {
        log_message("ERROR: Invalid IP format in ipToUint32.");
        return 0;
    }
    
    return addr.s_addr; 
}

std::string uint32ToIp(uint32_t ip_int) {
    
    struct in_addr addr;
    addr.s_addr = ip_int;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
    
    return std::string(ip_str);
}