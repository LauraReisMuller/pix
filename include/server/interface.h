#ifndef SERVER_INTERFACE_H
#define SERVER_INTERFACE_H

#pragma once

#include <iostream>

#include <queue>
#include <string>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "common/utils.h"

using namespace std;

class ServerInterface {
public:
    ServerInterface();
    ~ServerInterface();

    void start();
    void stop();

    // Sinaliza uma nova atualização + mensagem opcional de log
    void notifyUpdate(const string& logline = "");

private:
    thread th_;
    mutex m_;
    condition_variable cv_;
    queue<string> msgs_;
    atomic<bool> running_{false};

    void run();
};

// Instância global (usada por processing/discovery)
extern ServerInterface server_interface;

#endif // SERVER_INTERFACE_H
