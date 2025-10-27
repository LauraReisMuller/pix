// Interface do servidor: apresenta updates e sumários do banco.
// Esta componente recebe notificações (`notifyUpdate`) do processamento
// e imprime linhas de log e um resumo atual do estado (nº transações,
// total transferido, saldo total). Corre numa thread dedicada.

#include "server/interface.h"
#include "server/database.h"
#include "common/utils.h"

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
    {
        auto summary = server_db.getBankSummary();
        cout << get_timestamp_str()
                  << " num_transactions " << summary.num_transactions
                  << " total_transferred " << summary.total_transferred
                  << " total_balance " << summary.total_balance
                  << endl;
    }

    unique_lock<mutex> lk(m_);
    while (running_) {
        cv_.wait(lk, [&]{ return !running_ || !msgs_.empty(); });
        
        while (!msgs_.empty()) {
            auto line = msgs_.front(); msgs_.pop();
            lk.unlock();

            if (!line.empty()) {
                ostringstream oss;
                oss << get_timestamp_str() << " " << line;
                cout << oss.str() << endl;
            }

            auto summary = server_db.getBankSummary();
            ostringstream summary_oss;
            summary_oss << "num_transactions " << summary.num_transactions
                       << " total_transferred " << summary.total_transferred
                       << " total_balance " << summary.total_balance;
            cout << summary_oss.str() << endl;
            lk.lock();
        }
    }
}