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
        std::cerr << "ERRO: Uso correto: " << argv[0] << " <PORTA_UDP>" << std::endl;
        return EXIT_FAILURE;
    }
    
    int port;
    try {
        port = std::stoi(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "ERRO: Porta inválida: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Iniciando cliente na porta " << port << "..." << std::endl;

    //Iniciar a Fase de Descoberta
    ClientDiscovery client_disco(port);
    std::string server_ip = client_disco.discoverServer();

    if (server_ip.empty()) {
        std::cerr << "FALHA: Não foi possível descobrir o servidor após várias tentativas." << std::endl;
        return EXIT_FAILURE;
    }

    Interface client_interface;
    client_interface.displayDiscoverySuccess(server_ip);
    
    // Simulação do loop principal (que será substituído pela lógica de multithreading)
    // O cliente deve ler da entrada padrão (teclado) o próximo comando.
    std::cout << "O cliente está aguardando comandos (IP_DESTINO VALOR)..." << std::endl;
    
    // Por enquanto, o programa apenas aguarda a interrupção (Ctrl+C ou Ctrl+D)
    while(std::cin.good()) {
        
        std::string line;
        std::getline(std::cin, line);
    }
    
    std::cout << "Encerrando cliente." << std::endl;
    
    return EXIT_SUCCESS;
}