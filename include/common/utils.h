#ifndef UTILS_H
#define UTILS_H

#include <cstdint> 
#include <string>
#include <time.h>
#include <iostream>
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

using namespace std;

// Retorna o timestamp atual formatado como uma string.
string get_timestamp_str();

// Loga uma mensagem no console com timestamp.
void log_message(const char *msg);
void log_message_core(const char *msg);

uint32_t ipToUint32(const string& ip_str);

string uint32ToIp(uint32_t ip_int);

// Extrai o ID do último byte de um IP
int getIdFromIP(const string& ip_str);

// Obtém o IP local da máquina
string getMyIP();


#endif // UTILS_H