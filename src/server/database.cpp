#include "server/database.h"
#include "server/interface.h"

ServerDatabase server_db;  // Definição da instância global

bool ServerDatabase::makeTransaction(const string& origin_ip, const string& dest_ip, Packet packet) {
    log_message("Entered makeTransaction");

    // Garante que a aritmética em ponto flutuante assinado evite wraparound não assinado
    double amount = static_cast<double>(packet.req.value);

    // Valida antes de tocar nos saldos/histórico
    if (!validateTransaction(origin_ip, dest_ip, amount)) {
        log_message("Transaction validation failed. Aborting transaction.");
        return false;
    }

    // Atualiza saldos dos clientes, TEM QUE SER OPERAÇÃO ATÔMICA
    if(!updateClientBalance(origin_ip, -amount)) { log_message("Update origin client balance error."); }
    if(!updateClientBalance(dest_ip,  amount)) { log_message("Update destination client balance error."); }
    log_message("Updated balances");

    addTransaction(origin_ip, packet.seqn, dest_ip, amount);

    updateBankSummary();
    
    // Atualiza número de req
    if(!updateClientLastReq(origin_ip, packet.seqn)) {log_message("Update Req error.");}

    // Print BankSummary after a successful transaction
    BankSummary summary = getBankSummary();
    string log_message = " num transactions " + std::to_string(summary.num_transactions) + 
                         " total transferred " + std::to_string(summary.total_transferred) + 
                         " total balance " + std::to_string(summary.total_balance);
    server_interface.notifyUpdate(log_message);

    return true;
}

bool ServerDatabase::validateTransaction(const string& origin_ip, const string& dest_ip, double amount) const {
    if (amount <= 0.0) {
        log_message("validateTransaction: invalid amount (<= 0).");
        return false;
    }

    lock_guard<mutex> lock(client_table_mutex);
    auto it_orig = client_table.find(origin_ip);
    if (it_orig == client_table.end()) {
        log_message("validateTransaction: origin client not found.");
        return false;
    }

    auto it_dest = client_table.find(dest_ip);
    if (it_dest == client_table.end()) {
        log_message("validateTransaction: destination client not found.");
        return false;
    }

    if (it_orig->second.balance < amount) {
        log_message("validateTransaction: insufficient funds in origin account.");
        return false;
    }

    return true;
}

/* Tabela de clientes */

// Escrita
bool ServerDatabase::addClient(const string& ip_address) {
    lock_guard<mutex> lock(client_table_mutex);

    // Cliente já existe
    if (client_table.find(ip_address) != client_table.end()) {
        return false;
    }
    client_table.emplace(ip_address, Client(ip_address));
    return true;
}

// Leitura
Client* ServerDatabase::getClient(const string& ip_address) {
    lock_guard<mutex> lock(client_table_mutex);
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        // Exibe informações do cliente
        cout << "Client Key: " << it->second.ip << endl; // Ip e porta do cliente.
        cout << "Client Last_req: " << it->second.last_req << endl;
        cout << "Client Balance: " << it->second.balance << endl;
        return &(it->second);
    }
    return nullptr;
}

// Escrita
bool ServerDatabase::updateClientLastReq(const string& ip_address, int req_number) {
    lock_guard<mutex> lock(client_table_mutex);
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        it->second.last_req = req_number;
        return true;
    }
    return false;
}

// Escrita
bool ServerDatabase::updateClientBalance(const string& ip_address, double transaction_value) {
    lock_guard<mutex> lock(client_table_mutex);
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        it->second.balance += transaction_value;
        return true;
    }
    return false;
}

// Leitura
vector<Client> ServerDatabase::getAllClients() const {
    lock_guard<mutex> lock(client_table_mutex);
    vector<Client> clients;
    for (const auto& pair : client_table) {
        clients.push_back(pair.second);
    }
    return clients;
}

// Leitura 
double ServerDatabase::getClientBalance(const string& ip_address) {
    // NOTE: Se o seu amigo implementar o shared_mutex (Leitor/Escritor), 
    // esta função usará um shared_lock (acesso de leitura).
    lock_guard<mutex> lock(client_table_mutex);
    
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.balance;
    }
    // Retorna um valor inválido se o cliente não for encontrado (protocolo: cliente não existe)
    return -1.0; 
}

/* Tabela de transações */

// Escrita
int ServerDatabase::addTransaction(const string& origin_ip, int req_id, const string& destination_ip, double amount) {
    int tx_id = getNextTransactionId();
    lock_guard<mutex> lock(transaction_history_mutex);
    transaction_history.emplace_back(tx_id, origin_ip, req_id, destination_ip, amount);
    return tx_id;
}

// Leitura
vector<Transaction> ServerDatabase::getTransactionHistory() const {
    lock_guard<mutex> lock(transaction_history_mutex);
    return transaction_history;
}

// Leitura
Transaction* ServerDatabase::getTransactionById(int tx_id) {
    lock_guard<mutex> lock(transaction_history_mutex);
    for (auto& tx : transaction_history) {
        if (tx.id == tx_id) {
            return &tx;
        }
    }
    return nullptr;
}

/* Tabela de Resumo Bancário */

// Leitura
BankSummary ServerDatabase::getBankSummary() const {
    lock_guard<mutex> lock(bank_summary_mutex);
    return bank_summary;
}

// Escrita
void ServerDatabase::updateBankSummary() {
    lock_guard<mutex> lock1(bank_summary_mutex);
    lock_guard<mutex> lock2(transaction_history_mutex);
    lock_guard<mutex> lock3(client_table_mutex);
    
    bank_summary.num_transactions = transaction_history.size();
    
    double total = 0.0;
    for (const auto& tx : transaction_history) {
        total += tx.amount;
    }
    bank_summary.total_transferred = total;
    
    double balance_sum = 0.0;
    for (const auto& pair : client_table) {
        balance_sum += pair.second.balance;
    }
    bank_summary.total_balance = balance_sum;
}


/* Métodos Auxiliares */

bool ServerDatabase::clientExists(const string& ip_address) const {
    lock_guard<mutex> lock(client_table_mutex);
    return client_table.find(ip_address) != client_table.end();
}

double ServerDatabase::getTotalBalance() const {
    lock_guard<mutex> lock(client_table_mutex);
    double total = 0.0;
    for (const auto& pair : client_table) {
        total += pair.second.balance;
    }
    return total;
}

int ServerDatabase::getNextTransactionId() {
    lock_guard<mutex> lock(transaction_id_mutex);
    return next_transaction_id++;
}