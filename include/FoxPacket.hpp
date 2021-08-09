#pragma once

#include <map>

#include "FoxNet.hpp"
#include "ByteStream.hpp"

// we allow packets in memory to be up to 4kb in size
#define MAX_PACKET_SIZE 4096

namespace FoxNet {
    class FoxPeer;
    typedef Byte PktID;

    /*
    * reserved packet ids for internal packets
    * we have room for up to 256 different packet types (including user-defined packets)
    * to start your user-defined packet id enum, do like so:
    * enum {
    *     USERPACKET1 = PKTID_USER_PACKET_START,
    *     USERPACKET2,
    *     ...
    * }
    */
    typedef enum {
        PKTID_NONE, // invalid packet ID, probably means we're waiting for a packet
        // ======= SHARED PACKETS =======
        PKTID_VAR_LENGTH, // uint32_t (pkt body size) & uint8_t (pkt ID) follows
        // ======= CLIENT TO SERVER PACKETS =======
        C2S_HANDSHAKE, // sends info like FoxNet version & endian flag
        // ======= SERVER TO CLIENT PACKETS =======
        S2C_HANDSHAKE, // responds to C2S_HANDSHAKE, tells if the handshake is accepted
        PKTID_USER_PACKET_START, // marks the start of user packets
    } PEER_PACKET_ID;

    typedef void (*PktHandler)(FoxPeer *peer, void *udata);

    struct PacketInfo {
        size_t pSize;
        PktHandler handler;
        int subscriber; // only peers of type PEERTYPE can accept this packet
    };

    inline bool isBigEndian() {
        union {
            uint32_t i;
            Byte c[4];
        } _indxint = {0xDEADB33F};

        return _indxint.c[0] == 0xDE;
    }

    // use this to define your own packet types, subscriber is a PEERTYPE.
    void registerUserPacket(PktID uid, PktHandler handler, int subscriber, size_t pSize);
    size_t getPacketSize(PktID);
    PktHandler getPacketHandler(PktID);
    int getPacketType(PktID);
}