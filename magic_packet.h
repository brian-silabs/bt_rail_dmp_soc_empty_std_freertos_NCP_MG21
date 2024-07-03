#ifndef MAGIC_PACKET_H
#define MAGIC_PACKET_H

#include <stdint.h>

#define MAGIC_PACKET_DEFAULT_TTL       0x3

typedef enum {
    MAGIC_PACKET_SUCCESS = 0,
    MAGIC_PACKET_ERROR_FATAL, // Generic error, must look into src (should not be used)
    MAGIC_PACKET_ERROR_DROPPED, // Not a magic packet
    MAGIC_PACKET_ERROR_DUPLICATE, // Already received this packet
    MAGIC_PACKET_ERROR_DISABLED // Filter not enabled
} MagicPacketError_t;

// Structure of the application payload
typedef struct {
    uint8_t frameCounter;
    uint8_t status; // Bit 0 indicates if the origin is a border router
    uint8_t timeToLive;
} MagicPacketPayload_t;

void enableMagicPacketFilter(uint16_t monitoredPanId_a, uint8_t amBorderRouter_a);
void disableMagicPacketFilter(void);

// Functions to forge and decode 802.15.4 Magic packets
void createMagicPacket(uint16_t srcAddress_a, uint16_t destAddress_a, uint16_t panID_a, uint8_t *packetBuffer_a, const MagicPacketPayload_t *magicPayload_a);
MagicPacketError_t decodeMagicPacket(uint8_t *packetBuffer, uint8_t *wake);

#endif // MAGIC_PACKET_H
