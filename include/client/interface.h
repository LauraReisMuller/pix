#ifndef CLIENT_INTERFACE_H
#define CLIENT_INTERFACE_H

#pragma once

#include <iostream>
#include <sstream>

#include <queue>
#include <string>
#include <functional>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>

#include "common/utils.h"
#include "common/protocol.h"

using namespace std;

class ClientRequest;

class ClientInterface {
public:

    // O construtor deve receber uma referência ao RequestManager para delegar comandos
    ClientInterface(ClientRequest& request_manager);
    ~ClientInterface();

    void start();
    void stop();

    // Enfileira um ACK recebido para exibição
    void pushAck(const AckData& ack);

    // Mensagem após descoberta
    void displayDiscoverySuccess(const string& server_ip);

private:
    ClientRequest& request_manager_;

    thread in_thread_, out_thread_;
    mutex mutex_;
    condition_variable cv_;
    queue<AckData> acks_;
    atomic<bool> running_{false};

    void inputLoop();   // lê stdin
    void outputLoop();  // imprime acks
};

#endif // CLIENT_INTERFACE_H