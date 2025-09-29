#include "server/discovery.h"
#include <iostream>
#include <stdexcept>

// O servidor deve ser iniciado com a porta UDP como parâmetro: ./servidor 4000 
int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <PORT>" << std::endl;
        return 1;
    }
    
    int port;
    try {
        port = std::stoi(argv[1]);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid port number." << std::endl;
        return 1;
    } catch (const std::out_of_range& e) {
        std::cerr << "Port number out of range." << std::endl;
        return 1;
    }

    // Adicionar mensagem inicial obrigatória!

    try {
        // Inicializa o subsistema de Descoberta
        ServerDiscovery server_disco(port);
        
        // Inicia o loop de escuta (bloqueia o main, ou seria em uma thread separada)
        server_disco.startDiscoveryListener();
        
    } catch (const std::exception& e) {
        std::cerr << "Server Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}