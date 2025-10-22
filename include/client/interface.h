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

class ClientInterface {
public:
    using CommandHandler = function<void(const string& dest_ip, uint32_t value)>;

    explicit ClientInterface(CommandHandler onCommand);
    ~ClientInterface();

    void start();
    void stop();

    // Enfileira um ACK recebido para exibição
    void pushAck(const ClientAck& ack);

    // Mensagem após descoberta
    void displayDiscoverySuccess(const string& server_ip);

private:
    void inputLoop();   // lê stdin
    void outputLoop();  // imprime acks
    string nowTimestamp() const;

    CommandHandler onCommand_;
    thread in_th_, out_th_;
    mutex m_;
    condition_variable cv_;
    queue<ClientAck> acks_;
    atomic<bool> running_{false};
};

#endif // CLIENT_INTERFACE_H