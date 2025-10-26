#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include "common/protocol.h"
#include "common/utils.h"
#include "server/locks.h"
#include <atomic>
#include <cstring>
#define ERROR -1

using namespace std;

struct Client {
    string ip;
    int last_req;
    double balance;

    Packet last_ack_response;
    
    Client(const string& client_ip) : ip(client_ip), last_req(0), balance(100.0) {}
};

struct Transaction {
    int id;
    string origin_ip;
    int req_id;
    string destination_ip;
    double amount;

    Transaction(int next_transaction_id, const string& origin_ip, int req_id, const string& destination_ip, double amount)
        : id(next_transaction_id++), origin_ip(origin_ip), req_id(req_id), destination_ip(destination_ip), amount(amount) {}
};

struct BankSummary {
    int num_transactions;
    double total_transferred;
    double total_balance;
};

class ServerDatabase {
private:
    unordered_map<string, Client> client_table;
    mutable RWLock client_table_lock;
    
    vector<Transaction> transaction_history;
    mutable RWLock transaction_history_lock;
    
    BankSummary bank_summary;
    mutable RWLock bank_summary_lock;

    atomic<int> next_transaction_id;

public:
    ServerDatabase() : next_transaction_id(1) {}

    bool addClient(const string& ip_address);

    double getClientBalance(const string& ip_address);
    double getClientBalance_unsafe(const string& ip_address);

    bool updateClientBalance(const string& ip_address, double new_balance);
    bool updateClientBalance_unsafe(const string& ip_address, double new_balance);

    uint32_t getClientLastReq(const string& ip_address);

    bool updateClientLastReq(const string& ip_address, int req_number);
    bool updateClientLastReq_unsafe(const string& ip_address, int req_number);

    Packet getClientLastAck(const string& ip_address);
    
    bool updateClientLastAck_unsafe(const string& ip_address, const Packet& ack);
    bool updateClientLastAck(const string& ip_address, const Packet& ack);

    
    bool makeTransaction(const string& origin_ip, const string& dest_ip, Packet request);

    int addTransaction(const string& origin_ip, int req_id, const string& destination_ip, double amount);
    int addTransaction_unsafe(const string& origin_ip, int req_id, const string& destination_ip, double amount);

    BankSummary getBankSummary() const;
    void updateBankSummary_unsafe();
    void updateBankSummary();
    
    double getTotalBalance() const;
};
extern ServerDatabase server_db;
