// interface.cpp
#include "client/interface.h"
#include "common/utils.h"
#include <stdio.h>
#include <iostream>

void Interface::displayDiscoverySuccess(const std::string& server_ip) {
    
    //Imprimir data e hora
    print_timestamp();

    // IP do servidor
    std::cout << " server_addr " << server_ip << std::endl;
}