#include "client/discovery.h"
#include "client/interface.h"
#include "common/utils.h"
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

    Interface client_interface;
    client_interface.displayDiscoverySuccess(server_ip);
    
    // Simulação do loop principal (que será substituído pela lógica de multithreading)
    // O cliente deve ler da entrada padrão (teclado) o próximo comando.
    cout << "O cliente está aguardando comandos (IP_DESTINO VALOR)..." << endl;
    
    // Por enquanto, o programa apenas aguarda a interrupção (Ctrl+C ou Ctrl+D)
    while(cin.good()) {
        
        string line;
        getline(cin, line);
    }
    
    cout << "Encerrando cliente." << endl;
    
    return EXIT_SUCCESS;
}