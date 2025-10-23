#ifndef UTILS_H
#define UTILS_H

#include <cstdint> 
#include <string>
#include <string>
#include <time.h>
#include <string>
#include <iostream>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

// Retorna o timestamp atual formatado como uma string.
string get_timestamp_str();

// Loga uma mensagem no console com timestamp.
void log_message(const char *msg);

uint32_t ipToUint32(const std::string& ip_str);

string uint32ToIp(uint32_t ip_int);

#endif // UTILS_H