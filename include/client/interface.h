#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <functional>
#include <atomic>
//#include "utils.h"


using namespace std;

struct ClientAck {
    string server_ip;
    uint32_t seqn;
    string dest_ip;
    uint32_t value;
    uint32_t new_balance;
};

class ClientRequest;

class ClientInterface {
public:

    // O construtor deve receber uma referência ao RequestManager para delegar comandos
    ClientInterface(ClientRequest& request_manager);

    void start();
    void stop();

    // Enfileira um ACK recebido para exibição
    void pushAck(const ClientAck& ack);

    // Mensagem após descoberta
    void displayDiscoverySuccess(const string& server_ip);

private:
    ClientRequest& _request_manager;
    void inputLoop();   // lê stdin
    void outputLoop();  // imprime acks
    string nowTimestamp() const;

    thread in_th_, out_th_;
    mutex m_;
    condition_variable cv_;
    queue<ClientAck> acks_;
    atomic<bool> running_{false};
};

#endif // CLIENT_INTERFACE_H