#include "server/database.h"
#include "server/interface.h"

ServerDatabase server_db;  // Definição da instância global

/* === Transações === */

bool ServerDatabase::makeTransaction(const string& origin_ip, const string& dest_ip, Packet packet) {
    log_message("Entered makeTransaction (Atomic Commit)");

    double amount = static_cast<double>(packet.req.value);
    
    // --- 1. AQUISIÇÃO SIMULTÂNEA DOS LOCKS DE ESCRITA ---
    // Este bloco garante que o COMMIT de débito/crédito/histórico é ATÔMICO.
    WriteGuard client_lock(client_table_lock);
    WriteGuard history_lock(transaction_history_lock);
    WriteGuard summary_lock(bank_summary_lock); 
    
    // --- 2. VALIDAÇÃO ATÔMICA FINAL (INLINE, SOB O LOCK) ---
    
    auto it_orig = client_table.find(origin_ip);
    auto it_dest = client_table.find(dest_ip);

    // Validação
    if (it_orig == client_table.end() || it_dest == client_table.end()) {
        log_message("Transaction failed: Origin or Destination client not found.");
        return false;
    }
    if (it_orig->second.balance < amount || amount <= 0.0) {
        log_message("Transaction failed: Insufficient funds or invalid amount.");
        return false;
    }

    // --- 3. COMMIT ATÔMICO (Usando lógica _UNSAFE/Inline) ---
    
    // a) Atualiza Saldos (Chama versão _UNSAFE)
    updateClientBalance_unsafe(origin_ip, -amount);
    updateClientBalance_unsafe(dest_ip, amount);
    log_message("Updated balances"); 

    // b) Atualiza Histórico (Chama versão _UNSAFE)
    addTransaction_unsafe(origin_ip, packet.seqn, dest_ip, amount);
    log_message("depois de addtransaction");

    // c) Atualiza last_req (Chama versão _UNSAFE)
    updateClientLastReq_unsafe(origin_ip, packet.seqn);

    // d) Atualiza Resumo (Chama versão _UNSAFE)
    updateBankSummary_unsafe();

    // ATENÇÃO: Adicione o cálculo do final_balance AQUI, sob o lock!
    double balance_double = getClientBalance_unsafe(origin_ip); // Supondo que você crie esta versão
    
    Packet final_ack;
    final_ack.type = PKT_REQUEST_ACK;
    final_ack.seqn = packet.seqn; 
    final_ack.ack.new_balance = static_cast<uint32_t>(balance_double >= 0 ? balance_double : 0);

    updateClientLastAck_unsafe(origin_ip, final_ack);
    
    // --- 4. NOTIFICAÇÃO (APÓS A LIBERAÇÃO DOS LOCKS) ---
    
    // A chamada a getBankSummary aqui é segura, pois ela pega o ReadGuard.
    BankSummary summary = getBankSummary(); 
    string interface_msg = " num transactions " + to_string(summary.num_transactions) + 
                         " total transferred " + to_string(summary.total_transferred) + 
                         " total balance " + to_string(summary.total_balance);
    server_interface.notifyUpdate(interface_msg);

    return true;
}

/*bool ServerDatabase::validateTransaction(const string& origin_ip, const string& dest_ip, double amount) const {
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
}*/

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

    WriteGuard write_lock(client_table_lock);
    auto it = client_table.find(ip_address);

    if (it != client_table.end()) {
        it->second.last_req = req_number;
        return true;
    }
    return false;
}

bool ServerDatabase::updateClientLastReq_unsafe(const string& ip_address, int req_number) {

    auto it = client_table.find(ip_address);

    if (it != client_table.end()) {
        it->second.last_req = req_number;
        return true;
    }
    return false;
}


// Escrita
bool ServerDatabase::updateClientBalance(const string& ip_address, double transaction_value) {

    WriteGuard write_lock(client_table_lock);
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        it->second.balance += transaction_value;
        return true;
    }
    return false;
}

bool ServerDatabase::updateClientBalance_unsafe(const string& ip_address, double transaction_value) {

    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        it->second.balance += transaction_value;
        return true;
    }
    return false;
}

// Escrita
bool ServerDatabase::updateClientLastAck_unsafe(const string& ip_address, const Packet& ack) {
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        it->second.last_ack_response = ack;
        return true;
    }
    return false;
}

//Leitura
// Leitura 
Packet ServerDatabase::getClientLastAck(const string& ip_address) {
    ReadGuard read_lock(client_table_lock);

    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.last_ack_response;
    }
    // Retorna um pacote vazio (ou um pacote de erro) se não for encontrado
    Packet empty_ack;
    memset(&empty_ack, 0, sizeof(Packet));
    return empty_ack;
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

// Leitura 
double ServerDatabase::getClientBalance(const string& ip_address) {
    // NOTE: Se o seu amigo implementar o shared_mutex (Leitor/Escritor), 
    // esta função usará um shared_lock (acesso de leitura).
    ReadGuard read_lock(client_table_lock);
    
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.balance;
    }
    // Retorna um valor inválido se o cliente não for encontrado (protocolo: cliente não existe)
    return -1.0; 
}

double ServerDatabase::getClientBalance_unsafe(const string& ip_address) {
    // NOTE: Se o seu amigo implementar o shared_mutex (Leitor/Escritor), 
    // esta função usará um shared_lock (acesso de leitura).

    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.balance;
    }
    // Retorna um valor inválido se o cliente não for encontrado (protocolo: cliente não existe)
    return -1.0; 
}


// Leitura
uint32_t ServerDatabase::getClientLastReq(const string& ip_address) {
    // Usa ReadGuard para leitura, permitindo alta concorrência.
    ReadGuard read_lock(client_table_lock);

    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.last_req;
    }
    
    // Se o cliente existe (foi adicionado na Descoberta), mas o IP não foi encontrado
    // (o que não deveria acontecer), ou se a tabela estiver sendo inicializada,
    // retornamos 0 ou -1. Retornar 0 (o primeiro ID é 1) é mais seguro para o protocolo.
    return 0;
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


int ServerDatabase::addTransaction_unsafe(const string& origin_ip, int req_id, const string& destination_ip, double amount) {
    int tx_id = next_transaction_id.fetch_add(1);

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
    ReadGuard read_lock(bank_summary_lock);
    return bank_summary;
}

// Escrita
void ServerDatabase::updateBankSummary() {
    
    ReadGuard read_lock(client_table_lock); 
    ReadGuard summary_lock(bank_summary_lock); 
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

void ServerDatabase::updateBankSummary_unsafe() {
    
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