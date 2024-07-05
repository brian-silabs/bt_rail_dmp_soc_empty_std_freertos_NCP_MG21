#ifndef MAGIC_PACKET_H
#define MAGIC_PACKET_H

#include <stdint.h>

#define MAGIC_PACKET_PAYLOAD_LENGTH     3
#define MAGIC_PACKET_DEFAULT_TTL        0x3

typedef enum {
    MAGIC_PACKET_SUCCESS = 0,
    MAGIC_PACKET_ERROR_FATAL, // Generic error, must look into src (should not be used)
    MAGIC_PACKET_ERROR_DROPPED, // Not a magic packet
    MAGIC_PACKET_ERROR_DUPLICATE, // Already received this packet
    MAGIC_PACKET_ERROR_DISABLED // Filter not enabled
} MagicPacketError_t;

typedef enum {
    MAGIC_PACKET_EVENT_ENABLED = 0, // Init 
    MAGIC_PACKET_EVENT_DISABLED, // Deinit
    MAGIC_PACKET_EVENT_WAKE_RX, // Valid Magic packet received
    MAGIC_PACKET_EVENT_TX,// TX requested
} MagicPacketCallbackEvent_t;

// Structure of the init payload
typedef struct {
    uint16_t panId;
    uint8_t channel;
    uint8_t borderRouter;
} MagicPacketEnablePayload_t;

// Structure of the magic packet payload
typedef struct {
    uint8_t frameCounter;
    uint8_t status; // Bit 0 indicates if the origin is a border router
    uint8_t timeToLive;
} MagicPacketPayload_t;

void enableMagicPacketFilter(MagicPacketEnablePayload_t *enablePayload_a);
void disableMagicPacketFilter(void);

// Functions to forge and decode 802.15.4 Magic packets
void createMagicPacket(uint16_t srcAddress_a, uint16_t destAddress_a, uint16_t panID_a, uint8_t *packetBuffer_a, const MagicPacketPayload_t *magicPayload_a);
MagicPacketError_t decodeMagicPacket(uint8_t *packetBuffer);

MagicPacketError_t magicPacketCallback(MagicPacketCallbackEvent_t event, void *data);

#endif // MAGIC_PACKET_H
