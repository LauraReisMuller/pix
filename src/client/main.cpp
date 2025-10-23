#include "client/discovery.h"
#include "client/interface.h"
#include "client/request.h"
#include "common/utils.h"

#include <thread>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib> 


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

    cout << "Iniciando cliente na porta " << port << "..." << endl;

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
    client_interface.start(); 
    client_interface.displayDiscoverySuccess(server_ip);
    
    // Simulação do loop principal (que será substituído pela lógica de multithreading)
    cout << "O cliente está aguardando comandos (IP_DESTINO VALOR)..." << endl;
    
    // 4. Iniciar a Thread de Processamento de Requisições
    // Esta thread será a ÚNICA que chamará sendRequestWithRetry (lógica bloqueante)
    std::thread processing_thread(&ClientRequest::runProcessingLoop, &request_manager);

    // Esperar pelas threads (opcional, mas bom para evitar que o main termine)
    processing_thread.join();
    client_interface.stop(); // O stop é mais complexo com threads.

    return EXIT_SUCCESS;
}