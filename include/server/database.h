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
    string ip;         // Endereço IP do cliente
    int last_req;      // Número da última requisição processada *no cliente*
    double balance;    // Saldo atual do cliente

    //Armazena a última resposta enviada (para reenvio rápido em caso de duplicidade)
    Packet last_ack_response;
    
    Client(const string& client_ip) : ip(client_ip), last_req(0), balance(100.0) {}
};

struct Transaction {
    int id;                 // Número da transação
    string origin_ip;       // Endereço IP de origem
    int req_id;             // ID da requisição *no cliente*!
    string destination_ip;  // Endereço IP de destino
    double amount;          // Valor da transação

    Transaction(int next_transaction_id, const string& origin_ip, int req_id, const string& destination_ip, double amount)
        : id(next_transaction_id++), origin_ip(origin_ip), req_id(req_id), destination_ip(destination_ip), amount(amount) {}
};

struct BankSummary {
    int num_transactions;       // Número de transações total
    double total_transferred;   // Valor total transferido
    double total_balance;       // Saldo total do banco
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

    double getClientBalance(const string& ip_address);
    double getClientBalance_unsafe(const string& ip_address);

    bool updateClientBalance(const string& ip_address, double new_balance);
    bool updateClientBalance_unsafe(const string& ip_address, double new_balance);

    uint32_t getClientLastReq(const string& ip_address);

    bool updateClientLastReq(const string& ip_address, int req_number);
    bool updateClientLastReq_unsafe(const string& ip_address, int req_number);

    Packet getClientLastAck(const string& ip_address);
    
    bool updateClientLastAck_unsafe(const string& ip_address, const Packet& ack);

    
    // === Métodos para gerenciar transações ===
    bool makeTransaction(const string& origin_ip, const string& dest_ip, Packet request);

    int addTransaction(const string& origin_ip, int req_id, const string& destination_ip, double amount);
    int addTransaction_unsafe(const string& origin_ip, int req_id, const string& destination_ip, double amount);

    // === Métodos para estatísticas do banco ===
    BankSummary getBankSummary() const;
    void updateBankSummary_unsafe();
    void updateBankSummary();
    
    // === Métodos auxiliares ===
    double getTotalBalance() const;
};

// Instância única do banco de dados do servidor
extern ServerDatabase server_db;
