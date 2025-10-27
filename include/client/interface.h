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
    ClientInterface(ClientRequest& request_manager);
    ~ClientInterface();

    void start();
    void stop();

    void pushAck(const AckData& ack);

    void displayDiscoverySuccess(const string& server_ip);

private:
    ClientRequest& _request_manager;

    thread in_th_, out_th_;
    mutex m_;
    condition_variable cv_;
    queue<AckData> acks_;
    atomic<bool> running_{false};

    void inputLoop();
    void outputLoop();
};

#endif // CLIENT_INTERFACE_H