#include "server/database.h"
#include "server/interface.h"

ServerDatabase server_db;  // Definição da instância global

/* === Transações === */

bool ServerDatabase::makeTransaction(const string& origin_ip, const string& dest_ip, Packet packet) {
    {
        WriteGuard client_lock(client_table_lock);
        WriteGuard history_lock(transaction_history_lock);
        WriteGuard summary_lock(bank_summary_lock); 

        uint32_t amount = packet.req.value;
        
        auto it_orig = client_table.find(origin_ip);
        auto it_dest = client_table.find(dest_ip);
        bool clients_exist = (it_orig != client_table.end() && it_dest != client_table.end());
        
        if (!clients_exist) {
            // Se não existe, retorna falso ANTES de tentar ler saldo
            log_message("Transaction failed: Client not found.");
            return false; 
        }

        bool enough_balance = (it_orig->second.balance >= amount);
        bool valid_amount = (amount > 0);
    
        // Validação
        if (!clients_exist) {
            log_message("Transaction failed: Origin or Destination client not found.");
            updateClientLastReq_unsafe(origin_ip, packet.seqn);
            updateBankSummary_unsafe();
            return false;
        }
        if (!enough_balance || !valid_amount) {
            log_message("Transaction failed: Insufficient funds or invalid amount.");
            updateClientLastReq_unsafe(origin_ip, packet.seqn);
            updateBankSummary_unsafe();
            return false;
        }

        // --- 3. COMMIT ATÔMICO (Usando lógica _UNSAFE/Inline) ---
        
        updateClientBalance_unsafe(origin_ip, -amount);
        updateClientBalance_unsafe(dest_ip, amount);

        addTransaction_unsafe(origin_ip, packet.seqn, dest_ip, amount);

        updateClientLastReq_unsafe(origin_ip, packet.seqn);

        updateBankSummary_unsafe();

        uint32_t balance = getClientBalance_unsafe(origin_ip);
        
        Packet final_ack;
        final_ack.type = PKT_REQUEST_ACK;
        final_ack.seqn = packet.seqn; 
        final_ack.ack.new_balance = balance >= 0 ? balance : 0;

        updateClientLastAck_unsafe(origin_ip, final_ack);
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

    return true;
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

bool ServerDatabase::updateClientBalance(const string& ip_address, int32_t transaction_value) {
    WriteGuard write_lock(client_table_lock);

    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        it->second.balance += transaction_value;
        return true;
    }

    return false;
}

bool ServerDatabase::updateClientBalance_unsafe(const string& ip_address, int32_t transaction_value) {
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

bool ServerDatabase::updateClientLastAck(const string& ip_address, const Packet& ack) {
    WriteGuard write_lock(client_table_lock);
    
    auto it = client_table.find(ip_address);

    if (it != client_table.end()) {
        it->second.last_ack_response = ack;
        return true;
    }

    return false;
}

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

uint32_t ServerDatabase::getClientBalance(const string& ip_address) {
    ReadGuard read_lock(client_table_lock);
    
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.balance;
    }

    return ERROR; 
}

uint32_t ServerDatabase::getClientBalance_unsafe(const string& ip_address) {
    auto it = client_table.find(ip_address);
    if (it != client_table.end()) {
        return it->second.balance;
    }

    return ERROR; 
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
    // (o que não deveria acontecer), ou se a tabela estiver sendo inicializada, retornamos 0.
    return 0;
}

int ServerDatabase::addTransaction(const string& origin_ip, int req_id, const string& destination_ip, uint32_t amount) {
    int tx_id = next_transaction_id.fetch_add(1);
    WriteGuard write_lock(transaction_history_lock);

    transaction_history.emplace_back(tx_id, origin_ip, req_id, destination_ip, amount);

    return tx_id;
}


int ServerDatabase::addTransaction_unsafe(const string& origin_ip, int req_id, const string& destination_ip, uint32_t amount) {
    int tx_id = next_transaction_id.fetch_add(1);

    transaction_history.emplace_back(tx_id, origin_ip, req_id, destination_ip, amount);

    return tx_id;
}

/* Tabela de Resumo Bancário */

// Leitura
BankSummary ServerDatabase::getBankSummary() const {
    ReadGuard read_lock(bank_summary_lock);
    
    return bank_summary;
}

// Escrita / Leitura
void ServerDatabase::updateBankSummary_unsafe() {
    bank_summary.num_transactions = transaction_history.size();
    
    bank_summary.total_transferred = 0;
    for (const auto& tx : transaction_history) {
        bank_summary.total_transferred += tx.amount;
    }
    
    bank_summary.total_balance = 0;
    for (const auto& pair : client_table) {
        bank_summary.total_balance += pair.second.balance;
    }
}

// Escrita / Leitura
void ServerDatabase::updateBankSummary() {
    ReadGuard client_lock(client_table_lock);
    ReadGuard transaction_lock(transaction_history_lock);
    WriteGuard summary_lock(bank_summary_lock);

    bank_summary.num_transactions = transaction_history.size();
    
    bank_summary.total_transferred = 0;
    for (const auto& tx : transaction_history) {
        bank_summary.total_transferred += tx.amount;
    }
    
    bank_summary.total_balance = 0;
    for (const auto& pair : client_table) {
        bank_summary.total_balance += pair.second.balance;
    }
}

uint32_t ServerDatabase::getTotalBalance() const {
    ReadGuard read_lock(client_table_lock);

    uint32_t total = 0;
    for (const auto& pair : client_table) {
        total += pair.second.balance;
    }

    return total;
}

void ServerDatabase::forceClientBalance(const string& ip, uint32_t new_balance) {
    WriteGuard lock(client_table_lock);
    auto it = client_table.find(ip);
    if (it != client_table.end()) {
        it->second.balance = new_balance; // Sobrescreve sem validar
    }
}