// interface.cpp
#include "server/interface.h"
#include "server/databases.h"

ServerInterface server_interface;

ServerInterface::ServerInterface() {}
ServerInterface::~ServerInterface() { stop(); }

void ServerInterface::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    th_ = thread(&ServerInterface::run, this);
}

void ServerInterface::stop() {
    if (!running_) return;
    {
        lock_guard<mutex> lk(m_);
        running_ = false;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
}

void ServerInterface::notifyUpdate(const string& logline) {
    {
        lock_guard<mutex> lk(m_);
        if (!logline.empty()) msgs_.push(logline);
    }
    cv_.notify_one();
}

void ServerInterface::run() {
    // Mensagem inicial com dados do BankSummary
    {
        auto summary = server_db.getBankSummary();
        cout << get_timestamp_str()
                  << " num transactions " << summary.num_transactions
                  << " total transferred " << summary.total_transferred
                  << " total balance " << summary.total_balance
                  << endl;
    }

    unique_lock<mutex> lk(m_);
    while (running_) {
        cv_.wait(lk, [&]{ return !running_ || !msgs_.empty(); });
        
        while (!msgs_.empty()) {
            auto line = msgs_.front(); msgs_.pop();
            lk.unlock();

            // Imprime linha de log (ex.: req, dup, etc.)
            if (!line.empty()) {
                cout << get_timestamp_str() << " " << line << endl;
            }

            // Imprime resumo atualizado
            auto summary = server_db.getBankSummary();
            cout << "num transactions " << summary.num_transactions
                      << " total transferred " << summary.total_transferred
                      << " total balance " << summary.total_balance
                      << endl;

            lk.lock();
        }
    }
}