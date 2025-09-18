// Definições de structs e enums para os pacotes (DESCUBERTA, REQ, ACK…)

#ifndef PROTOCOL_H
#define PROTOCOL_H

// Exemplo de enum para tipos de pacotes
typedef enum {
    PKT_DISCOVER,
    PKT_REQ,
    PKT_ACK
} PacketType;

// Exemplo de struct para um pacote genérico
typedef struct {
    PacketType type;
    int id;
    char payload[256];
} Packet;

#endif // PROTOCOL_H
