#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
//#include "common/utils.h"

using namespace std;

struct Client {
    string ip;         // Endereço IP do cliente
    int last_req;      // Número da última requisição processada *no cliente*
    double balance;    // Saldo atual do cliente

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
    mutable mutex client_table_mutex;
    
    // Histórico de transações
    vector<Transaction> transaction_history;
    mutable mutex transaction_history_mutex;
    
    // Resumo/estatísticas do banco
    BankSummary bank_summary;
    mutable mutex bank_summary_mutex;

    // Contador para gerar IDs únicos de transação
    int next_transaction_id;
    mutable mutex transaction_id_mutex;

public:
    ServerDatabase() : next_transaction_id(1) {}

    // === Métodos para gerenciar clientes ===
    bool addClient(const string& ip_address);
    Client* getClient(const string& ip_address);
    bool updateClientLastReq(const string& ip_address, int req_number);
    bool updateClientBalance(const string& ip_address, double new_balance);
    vector<Client> getAllClients() const;
    
    // === Métodos para gerenciar transações ===
    int addTransaction(int next_transaction_id, const string& origin_ip, int req_id, const string& destination_ip, double amount);
    vector<Transaction> getTransactionHistory() const;
    Transaction* getTransactionById(int tx_id);

    // === Métodos para estatísticas do banco ===
    BankSummary getBankSummary() const;
    void updateBankSummary();
    
    // === Métodos auxiliares ===
    bool clientExists(const string& ip_address) const;
    double getTotalBalance() const;
    int getNextTransactionId();
};

// Instância única do banco de dados do servidor
static ServerDatabase server_db;
