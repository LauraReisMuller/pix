#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

typedef struct{
    uint32_t dest_addr; 
    uint32_t value;
} RequestData;

typedef struct {
    uint32_t seqn;
    uint32_t new_balance;
    uint32_t dest_addr;
    uint32_t value;
    uint32_t server_addr;
} AckData;

typedef enum {
    PKT_DISCOVER,
    PKT_DISCOVER_ACK,
    PKT_REQUEST,
    PKT_REQUEST_ACK
} PacketType;


typedef struct {
    uint16_t type;
    uint32_t seqn;
    
    union {
        RequestData req;
        AckData ack;
    };

} Packet;

#endif // PROTOCOL_H
