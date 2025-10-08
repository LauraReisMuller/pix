#include "server/discovery.h"
#include "common/utils.h"
#include <iostream>
#include <stdexcept>

// O servidor deve ser iniciado com a porta UDP como parâmetro: ./servidor 4000 
int main(int argc, char* argv[]) {

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <PORT>" << endl;
        return 1;
    }
    
    int port;
    try {
        port = stoi(argv[1]);
    } catch (const invalid_argument& e) {
        cerr << "Invalid port number." << endl;
        return 1;
    } catch (const out_of_range& e) {
        cerr << "Port number out of range." << endl;
        return 1;
    }

    // Adicionar mensagem inicial obrigatória!

    try {
        // Inicializa o subsistema de Descoberta
        ServerDiscovery server_disco(port);
        
        // Inicia o loop de escuta (bloqueia o main, ou seria em uma thread separada)
        server_disco.startDiscoveryListener();
        
    } catch (const exception& e) {
        cerr << "Server Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}