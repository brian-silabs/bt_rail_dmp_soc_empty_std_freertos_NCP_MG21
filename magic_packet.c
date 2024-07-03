#include "magic_packet.h"
#include <string.h>

#define MAX_PAYLOAD_LENGTH              128

#define HEADER_802154_LENGTH            9 // Check if better than sizeof
#define HEADER_802154_PANID_SHIFT       3
#define HEADER_802154_DEST_SHIFT        5
#define HEADER_802154_SRC_SHIFT         7

#define MAGIC_PACKET_PAYLOAD_LENGTH     3 // Check if better than sizeof
#define MAGIC_PACKET_FC                 0x8841 //Data Frame, No Security, No Frame Pending, No Ack Required, PanID compressed, 2003 ver, Short Dest Address, Short Source Address
#define MAGIC_PACKET_SRC_ADDRESS        0xFFFF
#define MAGIC_PACKET_DEST_ADDRESS       0xFFFF

#define MAGIC_PACKET_STATUS_BR_SHIFT    0
#define MAGIC_PACKET_STATUS_BR_MASK     0x0001

//Combination of these 3 should not be allowed, hence considered "magic"

// Structure of the 802.15.4 header
typedef struct {
    uint16_t frameControl;
    uint8_t seqNumber;
    uint16_t panID;
    uint16_t dstAddress;
    uint16_t srcAddress;
} IEEE802154_Header_t;

static uint8_t      filterEnabled_g             = 0;
static uint8_t      amBorderRouter_g            = 0;
static uint16_t     panId_g                     = 0xFFFF;
static uint8_t      header154LastFC_g           = 0xFF;//Access should be protected 
static uint8_t      magicPacketLastFC_g         = 0xFF;//Access should be protected 

static uint8_t      txBuffer[MAX_PAYLOAD_LENGTH] = {0x00};// Temp Tx Buffer

static uint8_t validateMagicPayloadFC(const MagicPacketPayload_t *magicPayload_a);
static void retransmitMagicPacket(const MagicPacketPayload_t *magicPayload_a);

void enableMagicPacketFilter(uint16_t monitoredPanId_a, uint8_t amBorderRouter_a)
{
    panId_g = monitoredPanId_a;
    amBorderRouter_g = amBorderRouter_a;
    filterEnabled_g = 1;
}

void disableMagicPacketFilter(void)
{
    filterEnabled_g = 0;
    amBorderRouter_g = 0;
    panId_g = 0xFFFF;
}

// Forge a magic 802.15.4 packet
void createMagicPacket(uint16_t srcAddress_a, uint16_t destAddress_a, uint16_t panID_a, uint8_t *packetBuffer_a, const MagicPacketPayload_t *magicPayload_a)
{
    IEEE802154_Header_t header;
    // MagicPacketPayload_t magicPayload;

    header.frameControl = MAGIC_PACKET_FC;
    header.seqNumber = 0;                       // Sequence number (can be incremented by the caller)
    header.panID = panID_a;                       // Needs to be filled from OT Pan ID
    header.dstAddress = destAddress_a;
    header.srcAddress = srcAddress_a;
    
    // magicPayload.frameCounter = ++magicPacketLastFC_g;
    // magicPayload.status &= ((borderRouter_a << MAGIC_PACKET_STATUS_BR_SHIFT) & MAGIC_PACKET_STATUS_BR_MASK);

    memcpy(packetBuffer_a, &header, sizeof(IEEE802154_Header_t));
    memcpy(packetBuffer_a + sizeof(IEEE802154_Header_t), magicPayload_a, sizeof(MagicPacketPayload_t));
}

// Decode an 802.15.4 packet
MagicPacketError_t decodeMagicPacket(uint8_t *packetBuffer_a, uint8_t *wake_a)
{
    IEEE802154_Header_t *header = (IEEE802154_Header_t *)packetBuffer_a;
    MagicPacketPayload_t *magicPayload = (MagicPacketPayload_t *)(packetBuffer_a + sizeof(IEEE802154_Header_t));

    MagicPacketError_t ret = MAGIC_PACKET_SUCCESS;

    if(filterEnabled_g)
    {
        *wake_a = 0;
        if( (MAGIC_PACKET_FC == header->frameControl)   
            && (panId_g == header->panID)               
            && (MAGIC_PACKET_SRC_ADDRESS == header->srcAddress)
            && (MAGIC_PACKET_DEST_ADDRESS == header->dstAddress)
            && (validateMagicPayloadFC(magicPayload))) // TODO : this may be done later, if we want to retransmit after wake up ?
        {
            // We are good to proceed with a wake up
            *wake_a = 1;
            if(magicPayload->timeToLive > 0){
                retransmitMagicPacket(magicPayload);
            }
        } else {
            ret = MAGIC_PACKET_ERROR_DROPPED;
        }
    } else {
        ret = MAGIC_PACKET_ERROR_DISABLED;
    }
    return ret;
}

// Decode the application payload
// There is a pb with this if nodes missed the 0xFF
static uint8_t validateMagicPayloadFC(const MagicPacketPayload_t *magicPayload_a) 
{
    //Check and validate Frame counter
    if( (magicPayload_a->frameCounter > magicPacketLastFC_g)
        || ((magicPayload_a->frameCounter > 0 ) && (magicPacketLastFC_g == 0xFF)))
    {
        return 1;
    } else 
    {
        return 0;
    }
}

static void retransmitMagicPacket(const MagicPacketPayload_t *magicPayload_a)
{
    createMagicPacket(MAGIC_PACKET_SRC_ADDRESS, MAGIC_PACKET_DEST_ADDRESS, panId_g, &txBuffer[1], magicPayload_a);
    txBuffer[0] = sizeof(IEEE802154_Header_t) + sizeof(MagicPacketPayload_t); // Separating size management as might be defferent in RAIL or OT
    //TODO TX(txBuffer);
}
