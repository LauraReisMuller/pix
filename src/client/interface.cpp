// interface.cpp
#include "client/interface.h"
#include <stdio.h>
#include <iostream>

#include <sstream>
#include <chrono>
#include <ctime>

ClientInterface::ClientInterface(CommandHandler onCommand)
    : onCommand_(move(onCommand)) {}

ClientInterface::~ClientInterface() { stop(); }

// Uma thread para input e outra para output:
// Input aguarda mensagens e envia para fila;
// Output processa mensagens de fila.
void ClientInterface::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    in_th_ = thread(&ClientInterface::inputLoop, this);
    out_th_ = thread(&ClientInterface::outputLoop, this);
}

// Para as threads de input e output desse cliente.
void ClientInterface::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    if (in_th_.joinable()) in_th_.join();
    if (out_th_.joinable()) out_th_.join();
}

void ClientInterface::pushAck(const ClientAck& ack) {
    {
        lock_guard<mutex> lk(m_);
        acks_.push(ack);
    }
    cv_.notify_one();
}

void ClientInterface::displayDiscoverySuccess(const string& server_ip) {
    cout << nowTimestamp() << " discovery ok server " << server_ip << endl;
}

string ClientInterface::nowTimestamp() const {
    using namespace chrono;
    auto tp = system_clock::now();
    time_t t = system_clock::to_time_t(tp);
    tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// Thread de input fica em loop lendo comandos do cliente no terminal.
void ClientInterface::inputLoop() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;

    while (running_ && getline(cin, line)) {
        if (line.empty()) continue;
        
        istringstream iss(line);
        string dest_ip;
        uint32_t value;

        if (!(iss >> dest_ip >> value)) {
            cerr << "input invalido. Use: IP_DESTINO VALOR\n";
            continue;
        }

        if (onCommand_) onCommand_(dest_ip, value);
    }
}

// Thread de output fica em loop imprimindo dados da requisição assim que ack é recebido.
void ClientInterface::outputLoop() {
    unique_lock<mutex> lk(m_);

    while (running_) {
        cv_.wait(lk, [&]{ return !running_ || !acks_.empty(); });
        
        while (!acks_.empty()) {
            auto ack = acks_.front(); acks_.pop();
            lk.unlock();

            cout << nowTimestamp()
                      << " server " << ack.server_ip
                      << " id req " << ack.seqn
                      << " dest " << ack.dest_ip
                      << " value " << ack.value
                      << " new balance " << ack.new_balance
                      << endl;

            lk.lock();
        }
    }
}