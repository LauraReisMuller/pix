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

    uint32_t final_balance_origin; 
    uint32_t final_balance_dest;
} ReplicationData;

typedef enum {
    PKT_DISCOVER,       // Mensagem de Descoberta (Cliente -> Servidor)
    PKT_DISCOVER_ACK,   // Resposta da Descoberta (Servidor -> Cliente)
    PKT_REQUEST,        // Requisicao de Transferencia (Cliente -> Servidor)
    PKT_REQUEST_ACK,    // Confirmacao de Requisicao (Servidor -> Cliente)

    PKT_REPLICATION_REQ, //Replica transação (Servidor Líder -> Servidor Backups) 
    PKT_REPLICATION_ACK,
    PKT_REP_CLIENT_REQ, //Replica criação de cliente (Servidor Líder -> Servidor Backups)
    PKT_REP_CLIENT_ACK,

    PKT_ELECTION,      // Inicia eleição (Servidor Backup -> Servidores Backups)
    PKT_ELECTION_OK,   // Resposta OK da eleição (Servidores Backup -> Servidor Líder)
    PKT_COORDINATOR,   // Coordenador eleito (Servidor Backup -> Servidores Backups)
    PKT_HEARTBEAT,     // Batimento cardíaco (Servidor Backup -> Servidores Backups)
    PKT_HEARTBEAT_ACK, // Resposta ao batimento cardíaco (Servidor Backup -> Servidores Backups)

    PKT_SERVER_DISCOVER,    //Descoberta de servidores
    PKT_SERVER_DISCOVER_ACK
} PacketType;

typedef struct {
    uint32_t candidate_id;
    uint32_t candidate_addr;
    uint16_t candidate_port;
} ElectionData;

typedef struct {
    uint32_t responder_id;
    uint32_t responder_addr;
    uint16_t responder_port;
} ElectionOkData;

typedef struct {
    uint32_t coordinator_id;
    uint32_t coordinator_addr;
    uint16_t coordinator_port;
    uint32_t timestamp;
} CoordinatorData;

typedef struct {
    uint32_t sender_id;
    uint32_t sender_addr;
    uint16_t sender_port;
    uint8_t is_primary;
} HeartbeatData;

//Dados para descoberta de servidor
typedef struct {
    int32_t id;           // ID do servidor (ex: 1, 2, 3...)
    int32_t replica_port; // Porta onde ele escuta réplicas (ex: 5000)
} ServerDiscoveryData;

// Estrutura de Pacote Genérico
typedef struct {
    uint16_t type;    // Tipo do pacote (PKT_REQUEST, PKT_ACK, etc.)
    uint32_t seqn;    // Número de sequência da requisição (ID no cliente)
    
    union {
        RequestData req;
        AckData ack;

        ReplicationData rep;

        ElectionData election;
        ElectionOkData election_ok;
        CoordinatorData coordinator;
        HeartbeatData heartbeat;
        ServerDiscoveryData server_discovery;
    };

} Packet;

#endif // PROTOCOL_H
