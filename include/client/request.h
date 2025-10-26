// request.h
#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include "common/protocol.h"
#include <string>
#include <mutex>
#include <queue>
#include <netinet/in.h>
#include <atomic>
#include <condition_variable>
#include <functional>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

using namespace std;

class ClientInterface;

using CommandHandler = function<void(const string& dest_ip, uint32_t value)>;

class ClientRequest{

public:
    ClientRequest(const string& server_ip, int port);
    ~ClientRequest();

    void setInterface(ClientInterface* interface);
    
    void enqueueCommand(const string& dest_ip, uint32_t value);
    
    void runProcessingLoop();
    void stopProcessing();

    bool isQueueEmpty() const;

private:
    string _server_ip;
    int _server_port;
    int _sockfd;
    struct sockaddr_in _server_addr;
    uint32_t _next_seqn; 
    
    queue<tuple<string, uint32_t>> _command_queue;
    mutable mutex _queue_mutex;
    condition_variable _queue_cv;
    atomic<bool> _running = true;

    ClientInterface* _interface;

    void setupSocket();

    bool sendRequestWithRetry(const Packet& request_packet);
};

#endif // CLIENT_REQUEST_H
