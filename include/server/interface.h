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
#include <sstream>

#include "common/utils.h"

using namespace std;

class ServerInterface {
public:
    ServerInterface();
    ~ServerInterface();

    void start();
    void stop();

    void notifyUpdate(const string& logline = "");

private:
    thread th_;
    mutex m_;
    condition_variable cv_;
    queue<string> msgs_;
    atomic<bool> running_{false};

    void run();
};

extern ServerInterface server_interface;

#endif // SERVER_INTERFACE_H
