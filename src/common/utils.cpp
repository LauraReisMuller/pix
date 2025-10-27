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
    //oi
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