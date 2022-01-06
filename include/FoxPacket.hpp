#pragma once

#include <map>
#include <chrono>

#include "FoxNet.hpp"
#include "ByteStream.hpp"

// we allow packets in memory to be up to 4kb in size
#define MAX_PACKET_SIZE 4096

namespace FoxNet {
    class FoxPeer;
    typedef Byte PktID;
    typedef uint16_t PktSize;

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
        // ======= PEER TO PEER PACKETS =======
        PKTID_PING,
        PKTID_PONG, // (sent in response to PKTID_PING)
        PKTID_VAR_LENGTH, // uint32_t (pkt body size) & uint8_t (pkt ID) follows
        PKTID_HANDSHAKE_REQ, // sends info like FoxNet version & endian flag
        PKTID_HANDSHAKE_RES, // responds to PKTID_HANDSHAKE_REQ, tells if the handshake is accepted
        // ======= CLIENT TO SERVER PACKETS =======
        // ======= SERVER TO CLIENT PACKETS =======
        PKTID_USER_PACKET_START, // marks the start of user packets
    } PEER_PACKET_ID;

    typedef void (*PktHandler)(FoxPeer *peer);
    typedef void (*PktVarHandler)(FoxPeer *peer, PktSize size);

    struct PacketInfo {
        union {
            PktHandler handler;
            PktVarHandler varhandler;
        };
        PktSize size;
        bool variable; // is a variable length packet?

        PacketInfo(): handler(nullptr), size(0), variable(false) {}
    };

    inline Byte isBigEndian() {
        union {
            uint32_t i;
            Byte c[4];
        } _indxint = {0xDEADB33F};

        return _indxint.c[0] == 0xDE;
    }

    inline int64_t getTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}