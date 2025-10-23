// utils.h
#ifndef UTILS_H
#define UTILS_H

using namespace std;

void print_timestamp();
void log_message(const char *msg);
uint32_t ipToUint32(const std::string& ip_str);
std::string uint32ToIp(uint32_t ip_int);

#endif // UTILS_H
