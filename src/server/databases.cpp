#include "server/databases.h"
#include <iostream>


bool ServerDatabase::addClient(const string& ip_address) {
    lock_guard<mutex> lock(client_table_mutex);

    if (client_table.find(ip_address) != client_table.end()) {
        // Cliente já existe
        return false;
    }
    client_table.emplace(ip_address, Client(ip_address));
    return true;
}

Client* ServerDatabase::getClient(const string& ip_address) {
    lock_guard<mutex> lock(client_table_mutex);
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        // Exibe informações do cliente
        cout << "Client Key: " << it->second.ip << endl; // Ip e porta do cliente.
        cout << "Client Last_req: " << it->second.last_req << endl;
        cout << "Client Balance: " << it->second.balance << endl;
        return &(it->second);
    }
    return nullptr;
}