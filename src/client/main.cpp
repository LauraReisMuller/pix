#include "client/discovery.h"
#include "client/interface.h"
#include "client/request.h"
#include <stdexcept>
#include <csignal>

using namespace std;
using namespace chrono;

//flag global de execucao para tratamento de sinal
atomic<bool> global_running{true};

//Handler de sinal (para ctrl+c)
void signal_handler(int signum) {

    if (signum == SIGINT) {

        //Sinaliza a todas as threads que o programa deve parar
        global_running = false; 
    }
}

int main(int argc, char* argv[]) {

    // O cliente deve ser iniciado com a porta UDP como parâmetro (ex: ./cliente 4000)
    if (argc < 2) {
        cerr << "ERRO: Uso correto: " << argv[0] << " <PORTA_UDP>" << endl;
        return EXIT_FAILURE;
    }
    
    int port;
    try {
        port = stoi(argv[1]);
    } catch (const exception& e) {
        cerr << "ERRO: Porta inválida: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    //Iniciar a Fase de Descoberta
    ClientDiscovery client_disco(port);
    string server_ip = client_disco.discoverServer();

    if (server_ip.empty()) {
        cerr << "FALHA: Não foi possível descobrir o servidor após várias tentativas." << endl;
        return EXIT_FAILURE;
    }

    ClientRequest request_manager(server_ip, port); 
    ClientInterface client_interface(request_manager); 
    request_manager.setInterface(&client_interface); 
    client_interface.displayDiscoverySuccess(server_ip);
    client_interface.start(); 
    
    // 4. Iniciar a Thread de Processamento de Requisições
    // Esta thread será a ÚNICA que chamará sendRequestWithRetry (lógica bloqueante)
    thread processing_thread(&ClientRequest::runProcessingLoop, &request_manager);

    // Espera até que o usuário pressione CTRL+D ou CTRL+C.
    while (global_running.load() && (cin.good() || !request_manager.isQueueEmpty())) {
        this_thread::sleep_for(milliseconds(100)); 
    }

    // Sinaliza todas as threads para pararem
    global_running = false; 
    request_manager.stopProcessing();

    client_interface.stop();
    if (processing_thread.joinable()) {
        processing_thread.join();
    }

    return EXIT_SUCCESS;
}