/**
 * Implementação da interface de linha de comandos do cliente.
 *
 * Responsabilidades:
 *  - ler entradas do utilizador (inputLoop) e enfileirar comandos para envio
 *  - apresentar ACKs recebidos do servidor (outputLoop)
 *  - gerir threads de input/output e sincronização entre elas
 */

#include "client/interface.h"
#include "client/request.h"

ClientInterface::ClientInterface(ClientRequest& request_manager)
    : _request_manager(request_manager) {}

ClientInterface::~ClientInterface() { stop(); }

void ClientInterface::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    in_th_ = thread(&ClientInterface::inputLoop, this);
    out_th_ = thread(&ClientInterface::outputLoop, this);
}

void ClientInterface::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    if (in_th_.joinable()) in_th_.join();
    if (out_th_.joinable()) out_th_.join();
}

void ClientInterface::pushAck(const AckData& ack) {
    {
        lock_guard<mutex> lk(m_);
        acks_.push(ack);
    }
    cv_.notify_one();
}

void ClientInterface::displayDiscoverySuccess(const string& server_ip) {
    cout << get_timestamp_str() << " server_addr " << server_ip << endl;
}

void ClientInterface::inputLoop() {
    using namespace std;

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

        _request_manager.enqueueCommand(dest_ip, value);
    }
}

void ClientInterface::outputLoop() {
    unique_lock<mutex> lk(m_);

    while (running_) {
        cv_.wait(lk, [&]{ return !running_ || !acks_.empty(); });
        
        while (!acks_.empty()) {
            auto ack = acks_.front(); acks_.pop();
            lk.unlock();

            char server_ip[INET_ADDRSTRLEN];
            char dest_ip[INET_ADDRSTRLEN];
            struct in_addr server_addr;
            server_addr.s_addr = ack.server_addr;
            inet_ntop(AF_INET, &server_addr, server_ip, INET_ADDRSTRLEN);
            if (ack.dest_addr != 0) {
                struct in_addr dest_addr;
                dest_addr.s_addr = ack.dest_addr;
                inet_ntop(AF_INET, &dest_addr, dest_ip, INET_ADDRSTRLEN);
            } else {
                strcpy(dest_ip, "N/A");
            }

            ostringstream oss;
            oss << get_timestamp_str()
                << " server " << server_ip
                << " id_req " << ack.seqn
                << " dest " << dest_ip
                << " value " << ack.value
                << " new_balance " << ack.new_balance;
            cout << oss.str() << endl;

            lk.lock();
        }
    }
}