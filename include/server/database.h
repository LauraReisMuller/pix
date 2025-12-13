#ifndef SERVER_DATABASE_H
#define SERVER_DATABASE_H

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
    uint32_t balance;

    Packet last_ack_response;
    
    Client(const string& client_ip) : ip(client_ip), last_req(0), balance(100.0) {}
};

struct Transaction {
    int id;
    string origin_ip;
    int req_id;
    string destination_ip;
    uint32_t amount;

    Transaction(int next_transaction_id, const string& origin_ip, int req_id, const string& destination_ip, uint32_t amount)
        : id(next_transaction_id++), origin_ip(origin_ip), req_id(req_id), destination_ip(destination_ip), amount(amount) {}
};

struct BankSummary {
    int num_transactions;
    uint32_t total_transferred;
    uint32_t total_balance;
};

class ServerDatabase {
private:
    // Tabela de clientes (hash table)
    unordered_map<string, Client> client_table;
    mutable RWLock client_table_lock;
    
    // Histórico de transações
    vector<Transaction> transaction_history;
    mutable RWLock transaction_history_lock;
    
    // Resumo/estatísticas do banco
    BankSummary bank_summary;
    mutable RWLock bank_summary_lock;

    // Contador para gerar IDs únicos de transação
    atomic<int> next_transaction_id;

public:
    ServerDatabase() : next_transaction_id(1) {}

    // === Métodos para gerenciar clientes ===
    bool addClient(const string& ip_address);

    uint32_t getClientBalance(const string& ip_address);
    uint32_t getClientBalance_unsafe(const string& ip_address);

    bool updateClientBalance(const string& ip_address, int32_t transaction_value);
    bool updateClientBalance_unsafe(const string& ip_address, int32_t transaction_value);

    uint32_t getClientLastReq(const string& ip_address);

    bool updateClientLastReq(const string& ip_address, int req_number);
    bool updateClientLastReq_unsafe(const string& ip_address, int req_number);

    Packet getClientLastAck(const string& ip_address);
    
    bool updateClientLastAck_unsafe(const string& ip_address, const Packet& ack);
    bool updateClientLastAck(const string& ip_address, const Packet& ack);

    
    // === Métodos para gerenciar transações ===
    bool makeTransaction(const string& origin_ip, const string& dest_ip, Packet request);

    int addTransaction(const string& origin_ip, int req_id, const string& destination_ip, uint32_t amount);
    int addTransaction_unsafe(const string& origin_ip, int req_id, const string& destination_ip, uint32_t amount);

    // === Métodos para estatísticas do banco ===
    BankSummary getBankSummary() const;
    void updateBankSummary_unsafe();
    void updateBankSummary();
    
    uint32_t getTotalBalance() const;

    void forceClientBalance(const string& ip, uint32_t new_balance); 
};

// Instância única do banco de dados do servidor
extern ServerDatabase server_db;

#endif // SERVER_DATABASE_H
