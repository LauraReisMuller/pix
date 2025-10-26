#include <thread>
#include <vector>
#include <iostream>
#include <cassert>
#include <random>
#include "server/database.h"

extern ServerDatabase server_db;

// Simula transações entre dois clientes
void simulate_transactions(const std::string& from, const std::string& to, int n_ops) {
    for (int i = 0; i < n_ops; ++i) {
        Packet p{};
        p.type = PKT_REQUEST;
        p.seqn = i + 1;
        p.req.value = rand() % 10 + 1; // valor aleatório
        inet_pton(AF_INET, to.c_str(), &p.req.dest_addr);

        server_db.makeTransaction(from, to, p);
    }
}

// Simula consultas de saldo concorrentes
void simulate_balance_queries(const std::string& ip, int n_ops) {
    for (int i = 0; i < n_ops; ++i) {
        double bal = server_db.getClientBalance(ip);
        (void)bal;
    }
}

// Simula adição de novos clientes (descoberta)
void simulate_discovery(int start_id, int count) {
    for (int i = 0; i < count; ++i) {
        std::string ip = "10.0.0." + std::to_string(start_id + i);
        server_db.addClient(ip);  // Cada cliente novo inicia com 100 reais
    }
}

int main() {
    // Clientes iniciais
    server_db.addClient("10.0.0.1");
    server_db.addClient("10.0.0.2");
    server_db.addClient("10.0.0.3");

    const int N_THREADS = 8;
    const int OPS_PER_THREAD = 1000;

    std::vector<std::thread> threads;

    // Threads de transações
    threads.emplace_back(simulate_transactions, "10.0.0.1", "10.0.0.2", OPS_PER_THREAD);
    threads.emplace_back(simulate_transactions, "10.0.0.2", "10.0.0.3", OPS_PER_THREAD);
    threads.emplace_back(simulate_transactions, "10.0.0.3", "10.0.0.1", OPS_PER_THREAD);
    threads.emplace_back(simulate_transactions, "10.0.0.1", "10.0.0.3", OPS_PER_THREAD);

    // Threads de consultas
    threads.emplace_back(simulate_balance_queries, "10.0.0.1", OPS_PER_THREAD);
    threads.emplace_back(simulate_balance_queries, "10.0.0.2", OPS_PER_THREAD);

    // Threads de descoberta
    threads.emplace_back(simulate_discovery, 100, 20);  // 20 novos clientes → 20*100
    threads.emplace_back(simulate_discovery, 200, 20);  // 20 novos clientes → 20*100

    for (auto& t : threads) {
        t.join();
    }

    // Cálculo do total esperado
    double expected_total = 3*100 + 40*100; // 3 clientes iniciais + 40 clientes descobertos
    double total = server_db.getTotalBalance();

    std::cout << "Total final: " << total << " (esperado: " << expected_total << ")" << std::endl;

    // Pequena tolerância por possível concorrência
    assert(total >= expected_total - 10 && total <= expected_total + 10);

    std::cout << "✅ Teste de thread safety concluído com sucesso!" << std::endl;
    return 0;
}
