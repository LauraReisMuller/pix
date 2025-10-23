// request.cpp
#include "client/request.h"
#include "common/protocol.h"
#include "common/utils.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

//Definicoes para o RRA (timeout/retry)
#define RRA_TIMEOUT_MS 10 
#define MAX_RETRIES 10


/*---Construtor e Setup ---*/

ClientRequest::ClientRequest(const std::string& server_ip, int port)
    : _server_ip(server_ip), _server_port(port), _sockfd(-1), _next_seqn(1), _interface(nullptr){
    
    // Inicializa o endereço do servidor
    memset(&_server_addr, 0, sizeof(_server_addr));
    _server_addr.sin_family = AF_INET;
    _server_addr.sin_port = htons(_server_port);
    inet_pton(AF_INET, _server_ip.c_str(), &(_server_addr.sin_addr));
    
    setupSocket();
}

ClientRequest::~ClientRequest() {
    if (_sockfd >= 0) {
        close(_sockfd);
    }
}

void ClientRequest::setInterface(ClientInterface* interface) {
    if (!interface) {
        log_message("CRITICAL ERROR: Attempted to set null interface pointer.");
        throw std::runtime_error("Null interface pointer.");
    }
    _interface = interface;
}

void ClientRequest::setupSocket() {
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0) {
        log_message("ERROR opening request socket");
        throw std::runtime_error("Failed to open request socket.");
    }
    // O cliente não precisa de bind, pois a porta de origem é aleatória (efêmera)
    log_message("Request socket created.");
}


void ClientRequest::stopProcessing() {
    log_message("RequestManager received stop signal. Stopping processing loop.");
    
    //Sinaliza a flag de parada
    _running = false; 
    //Acorda a thread que está bloqueada em cv.wait()
    _queue_cv.notify_all(); 
}

/*--- Sincronização (Fila Thread-Safe) ---*/

void ClientRequest::enqueueCommand(const std::string& dest_ip, uint32_t value) {
    {
        //Esta função é chamada pela thread de input da interface
        std::lock_guard<std::mutex> lk(_queue_mutex);
        _command_queue.push({dest_ip, value});
        log_message("Command enqueued. Notifying processing loop.");
    }
    _queue_cv.notify_one(); //Notifica a thread de processamento
}

/* Lógica bloqueante de envio (RRA) ---*/

bool ClientRequest::sendRequestWithRetry(const Packet& initial_request) {
    
    Packet current_request = initial_request;
    
    for (int retry_count = 0; retry_count < MAX_RETRIES; ++retry_count) {
        
        if (retry_count > 0) {
             //Notificação da retransmissão pode ir para o log ou para a interface
             std::string msg = "Retransmitting request ID: " + std::to_string(current_request.seqn);
             log_message(msg.c_str());
        }

        //1.Envio da Requisição
        ssize_t sent_bytes = sendto(_sockfd, (const char*)&current_request, sizeof(Packet), 0, 
                                    (const struct sockaddr*)&_server_addr, sizeof(_server_addr));
        
        if (sent_bytes < 0) {
            log_message("ERROR sending request.");
            //Falha grave, tenta novamente
            continue; 
        }

        //2.Aguardo do ACK com timeout (usando select)
        fd_set read_fds;
        struct timeval tv;
        
        FD_ZERO(&read_fds);
        FD_SET(_sockfd, &read_fds);

        //Timeout de 10ms
        tv.tv_sec = 0;
        tv.tv_usec = RRA_TIMEOUT_MS * 1000; 

        int retval = select(_sockfd + 1, &read_fds, NULL, NULL, &tv);

        if (retval == -1) {
            log_message("ERROR in select() during ACK wait.");
            continue; 
        } else if (retval == 0) {
            //timeout! O loop continua e reenvia a Requisição.
            log_message("ACK timeout. Retrying...");
            continue; 
        } else {
            //3.ACK recebido
            Packet ack_packet;
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);

            ssize_t received_bytes = recvfrom(_sockfd, (char*)&ack_packet, sizeof(Packet), 0, 
                                             (struct sockaddr*)&from_addr, &from_len);

            if (received_bytes < 0) {
                 log_message("ERROR on recvfrom ACK.");
                 continue;
            }

            //4.Validação do ACK
            if (ack_packet.type == PKT_REQUEST_ACK && ack_packet.seqn == current_request.seqn) {
            
                //Notificar a thread de output da interface
                ClientAck ack_data;
                ack_data.server_ip = _server_ip;
                ack_data.seqn = current_request.seqn;
                ack_data.dest_ip = uint32ToIp(current_request.req.dest_addr);
                ack_data.value = current_request.req.value;
                ack_data.new_balance = ack_packet.ack.new_balance;
                
                _interface->pushAck(ack_data);
                
                log_message("ACK received and processed successfully.");
                return true; //Sucesso: sai do laço de reenvio
                
            } else if (ack_packet.type == PKT_REQUEST_ACK && ack_packet.seqn < current_request.seqn) {
                // Cenário de ACK Duplicado/Atrasado (o cliente já esperava o próximo)
                // O servidor geralmente lida com isso. Aqui o cliente pode ignorar ou logar.
                log_message("Received delayed/duplicate ACK. Ignoring.");
                continue;
            }
             else {
                log_message("Received unexpected packet type or sequence number. Ignoring.");
                continue;
            }
        }
    }

    // Falha total: estourou o número máximo de tentativas
    log_message("CRITICAL: Failed to receive ACK after maximum retries. Aborting command.");
    return false;
}

/*--- Loop Principal de Processamento ---*/
void ClientRequest::runProcessingLoop() {
    
    while (_running) {
        std::unique_lock<std::mutex> lk(_queue_mutex);
        
        // Espera até que haja um comando na fila ou o cliente esteja parando
        _queue_cv.wait(lk, [&]{ return !_command_queue.empty() || !_running; });

        if (!_running) break; // Sai se o cliente estiver parando

        // Pega o próximo comando da fila (IP_DESTINO, VALOR)
        auto [dest_ip, value] = _command_queue.front();
        _command_queue.pop();
        lk.unlock();
        
        //1.Prepara o pacote de Requisição com o próximo ID sequencial
        Packet request_packet;
        request_packet.type = PKT_REQUEST;
        request_packet.seqn = _next_seqn;
        request_packet.req.dest_addr = ipToUint32(dest_ip);
        request_packet.req.value = value;
        
        //2. Chama a lógica bloqueante de envio e reenvio
        bool success = sendRequestWithRetry(request_packet);

        if (success) {
            //3.Incrementa o ID apenas após o ACK ser recebido com sucesso!
            _next_seqn++; 
        }

        // Se falhar, o cliente ainda precisa usar o mesmo _next_seqn na próxima tentativa (que o usuário digitar),
        // mas como a fila está vazia, ele esperará o próximo comando. 
    }
}




