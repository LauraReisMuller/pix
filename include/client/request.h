// request.h
#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include "common/protocol.h"
#include "client/interface.h"
#include <string>
#include <mutex>
#include <queue>
#include <netinet/in.h>

//Definir o handler de comandos para ser usado na Interface. Esta função será o callback que a thread de input chama.
using CommandHandler = std::function<void(const std::string& dest_ip, uint32_t value)>;

class ClientRequest{

public:

    //Construtor inicializa com o IP do servidor (descoberto) 
    ClientRequest(const std::string& server_ip, int port);
    ~ClientRequest();

    void setInterface(ClientInterface* interface);
    
    //Funcao chamada pela thread de input da Interface
    void enqueueCommand(const std::string& dest_ip, uint32_t value);
    
    //Loop principal de envio (com lógica bloqueante)
    void runProcessingLoop();
    void stopProcessing();

private:

    //Variáveis de estado
    std::string _server_ip;
    int _server_port;
    int _sockfd;
    struct sockaddr_in _server_addr;
    uint32_t _next_seqn; //Proximo ID a ser usado (comeca em 1)
    
    //Sincronizacao e fila 
    std::queue<std::tuple<std::string, uint32_t>> _command_queue; //Fila de comandos do usuário
    std::mutex _queue_mutex;
    std::condition_variable _queue_cv;
    std::atomic<bool> _running = true;

    //Referencia à interface para notificar a thread de saída
    ClientInterface* _interface;

    //Metodos de Comunicação
    void setupSocket();

    //Logica bloqueante principal (envio, timeout e reenvio)
    bool sendRequestWithRetry(const Packet& request_packet);

};

#endif // CLIENT_REQUEST_H
