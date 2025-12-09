// Definições de structs e enums para os pacotes (DESCUBERTA, REQ, ACK…)

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint> 

//Requisicao de transferencia
typedef struct{
    uint32_t dest_addr; // Endereço IP do cliente destino 
    uint32_t value;     // Valor da transferência
}RequestData;

//Resposta (Acknowledgement)
typedef struct {
    uint32_t seqn;        // Número de sequência (ID) que está sendo feito o ACK
    uint32_t new_balance; // Novo saldo do cliente origem
    uint32_t dest_addr;   // IP do cliente destino
    uint32_t value;       // Valor da transação
    uint32_t server_addr;
} AckData;

//Estrutura para replicação
typedef struct {
    uint32_t origin_addr; // IP de quem pagou
    uint32_t dest_addr;   // IP de quem recebeu
    uint32_t value;       // Valor
} ReplicationData;

typedef enum {
    PKT_DISCOVER,       // Mensagem de Descoberta (Cliente -> Servidor)
    PKT_DISCOVER_ACK,   // Resposta da Descoberta (Servidor -> Cliente)
    PKT_REQUEST,        // Requisicao de Transferencia (Cliente -> Servidor)
    PKT_REQUEST_ACK,    // Confirmacao de Requisicao (Servidor -> Cliente)
    PKT_REPLICATION_REQ, //Replica transação (Servidor Líder -> Servidor Backups) 
    PKT_REPLICATION_ACK,
    PKT_REP_CLIENT_REQ, //Replica criação de cliente (Servidor Líder -> Servidor Backups)
    PKT_REP_CLIENT_ACK
} PacketType;

//Estrutura de Pacote Genérico

typedef struct {
    uint16_t type;    // Tipo do pacote (PKT_REQUEST, PKT_ACK, etc.)
    uint32_t seqn;    // Número de sequência da requisição (ID no cliente)
    
    
    union {
        RequestData req;
        AckData ack;
        ReplicationData rep;
    };

} Packet;

#endif // PROTOCOL_H
