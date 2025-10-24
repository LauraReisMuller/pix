// request.h
#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include "common/protocol.h"
#include <string>
#include <mutex>
#include <queue>
#include <netinet/in.h>
#include <atomic>
#include <condition_variable>
#include <functional>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

using namespace std;

class ClientInterface;

//Definir o handler de comandos para ser usado na Interface. Esta função será o callback que a thread de input chama.
using CommandHandler = function<void(const string& dest_ip, uint32_t value)>;

class ClientRequest{

public:

    //Construtor inicializa com o IP do servidor (descoberto) 
    ClientRequest(const string& server_ip, int port);
    ~ClientRequest();

    void setInterface(ClientInterface* interface);
    
    //Funcao chamada pela thread de input da Interface
    void enqueueCommand(const string& dest_ip, uint32_t value);
    
    //Loop principal de envio (com lógica bloqueante)
    void runProcessingLoop();
    void stopProcessing();

private:

    //Variáveis de estado
    string _server_ip;
    int _server_port;
    int _sockfd;
    struct sockaddr_in _server_addr;
    uint32_t _next_seqn; //Proximo ID a ser usado (comeca em 1)
    
    //Sincronizacao e fila 
    queue<tuple<string, uint32_t>> _command_queue; //Fila de comandos do usuário
    mutex _queue_mutex;
    condition_variable _queue_cv;
    atomic<bool> _running = true;

    //Referencia à interface para notificar a thread de saída
    ClientInterface* _interface;

    //Metodos de Comunicação
    void setupSocket();

    //Logica bloqueante principal (envio, timeout e reenvio)
    bool sendRequestWithRetry(const Packet& request_packet);

};

#endif // CLIENT_REQUEST_H
