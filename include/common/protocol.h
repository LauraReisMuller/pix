// Definições de structs e enums para os pacotes (DESCUBERTA, REQ, ACK…)

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint> // Incluir para usar uint16_t, uint32_t

//Requisicao de transferencia
typedef struct{
    uint32_t dest_addr; // Endereço IP do cliente destino (em formato network order)
    uint32_t value;     // Valor da transferência
}RequestData;

//Resposta (Acknowledgement)
typedef struct {
    uint32_t seqn;        // Número de sequência (ID) que está sendo feito o ACK
    uint32_t new_balance; // Novo saldo do cliente origem
} AckData;

typedef enum {
    PKT_DISCOVER,       // Mensagem de Descoberta (Cliente -> Servidor)
    PKT_DISCOVER_ACK,   // Resposta da Descoberta (Servidor -> Cliente)
    PKT_REQUEST,        // Requisicao de Transferencia (Cliente -> Servidor)
    PKT_REQUEST_ACK     // Confirmacao de Requisicao (Servidor -> Cliente)
} PacketType;

//Estrutura de Pacote Genérico

typedef struct {
    uint16_t type;    // Tipo do pacote (PKT_REQUEST, PKT_ACK, etc.)
    uint32_t seqn;    // Número de sequência da requisição (ID no cliente)
    
    // Union permite que o pacote armazene diferentes estruturas no mesmo espaço
    union {
        RequestData req;
        AckData ack;
    };

} Packet;

#endif // PROTOCOL_H
