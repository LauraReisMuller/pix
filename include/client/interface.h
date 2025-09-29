#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#include <string>

class Interface {
public:
   
    void displayDiscoverySuccess(const std::string& server_ip);
};

#endif // CLIENT_INTERFACE_H