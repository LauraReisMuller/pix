#include "server/database.h"
#include "server/interface.h"

ServerDatabase server_db;  // Definição da instância global

/* === Transações === */

bool ServerDatabase::makeTransaction(const string& origin_ip, const string& dest_ip, Packet packet) {
    log_message("Entered makeTransaction");

    // Garante que a aritmética em ponto flutuante assinado evite wraparound não assinado
    double amount = static_cast<double>(packet.req.value);

    // Validação com READ lock
    {
        ReadGuard read_lock(client_table_lock);
        
        if (!validateTransaction(origin_ip, dest_ip, amount)) {
            log_message("Transaction validation failed inside makeTransaction.");
            return false;
        }
    }

    // Modificação com WRITE lock
    {
        WriteGuard write_lock(client_table_lock);

        if(!updateClientBalance(origin_ip, -amount)) { log_message("Update origin client balance error."); }
        if(!updateClientBalance(dest_ip,  amount)) { log_message("Update destination client balance error."); }
        log_message("Updated balances");
    }


    addTransaction(origin_ip, packet.seqn, dest_ip, amount);

    updateBankSummary();
    
    // Atualiza número de req
    if(!updateClientLastReq(origin_ip, packet.seqn)) {log_message("Update Req error.");}

    // Print BankSummary after a successful transaction
    BankSummary summary = getBankSummary();
    string log_message = " num transactions " + to_string(summary.num_transactions) + 
                         " total transferred " + to_string(summary.total_transferred) + 
                         " total balance " + to_string(summary.total_balance);
    server_interface.notifyUpdate(log_message);

    return true;
}

bool ServerDatabase::validateTransaction(const string& origin_ip, const string& dest_ip, double amount) const {
    if (amount <= 0.0) {
        log_message("validateTransaction: invalid amount (<= 0).");
        return false;
    }

    ReadGuard read_lock(client_table_lock);
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

/* === Tabela de Clientes === */

bool ServerDatabase::addClient(const string& ip_address) {
    WriteGuard write_lock(client_table_lock);

    bool clientExists = (client_table.find(ip_address) != client_table.end());

    if (clientExists) {
        return false;
    }

    client_table.emplace(ip_address, Client(ip_address));
/*
    {
        lock_guard<mutex> lock(bank_summary_mutex);
        bank_summary.total_balance += 100.0;  // Saldo inicial do cliente
    }
*/
    return true;
}


Client* ServerDatabase::getClient(const string& ip_address) {
    ReadGuard read_lock(client_table_lock);

    bool clientExists = (client_table.find(ip_address) != client_table.end());

    if (clientExists) {
        // Exibe informações do cliente
        cout << "Client Key: " << client_table[ip_address].ip << endl; // Ip e porta do cliente.
        cout << "Client Last_req: " << client_table[ip_address].last_req << endl;
        cout << "Client Balance: " << client_table[ip_address].balance << endl;
        return &(client_table[ip_address]);
    }

    return nullptr;
}

// Escrita
bool ServerDatabase::updateClientLastReq(const string& ip_address, int req_number) {
    WriteGuard write_lock(client_table_lock);

    bool clientExists = (client_table.find(ip_address) != client_table.end());

    if (clientExists) {
        client_table[ip_address].last_req = req_number;
        return true;
    }
    return false;
}

// Escrita
bool ServerDatabase::updateClientBalance(const string& ip_address, double transaction_value) {
    WriteGuard write_lock(client_table_lock);

    bool clientExists = (client_table.find(ip_address) != client_table.end());

    if (clientExists) {
        client_table[ip_address].balance += transaction_value;
        return true;
    }
    return false;
}

// Leitura
vector<Client> ServerDatabase::getAllClients() const {
    ReadGuard read_lock(client_table_lock);
    vector<Client> clients;

    for (const auto& pair : client_table) {
        clients.push_back(pair.second);
    }

    return clients;
}

/* Tabela de transações */

// Escrita
int ServerDatabase::addTransaction(const string& origin_ip, int req_id, const string& destination_ip, double amount) {
    int tx_id = next_transaction_id.fetch_add(1);

    WriteGuard write_lock(transaction_history_lock);
    transaction_history.emplace_back(tx_id, origin_ip, req_id, destination_ip, amount);

/*
    // Atualiza estatísticas do banco
    {
        lock_guard<mutex> lock(bank_summary_mutex);
        bank_summary.num_transactions++;
        bank_summary.total_transferred += amount;
    }
*/
    return tx_id;
}

// Leitura
vector<Transaction> ServerDatabase::getTransactionHistory() const {
    ReadGuard read_lock(transaction_history_lock);
    return transaction_history;
}

// Leitura
Transaction* ServerDatabase::getTransactionById(int tx_id) {
    ReadGuard read_lock(transaction_history_lock);

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
    ReadGuard read_lock(client_table_lock);
    lock_guard<mutex> summary_lock(bank_summary_mutex);
    
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

double ServerDatabase::getTotalBalance() const {
    ReadGuard read_lock(client_table_lock);

    double total = 0.0;
    for (const auto& pair : client_table) {
        total += pair.second.balance;
    }

    return total;
}