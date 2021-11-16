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

    void Init();
    void Cleanup();

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
        PKTID_VAR_LENGTH, // uint32_t (pkt body size) & uint8_t (pkt ID) follows
        PKTID_CONTENTSTREAM_REQUEST, // requests to open a content stream [uint32_t content size] [uint16_t content id] [uint8_t type] [sha256 hash]
        PKTID_CONTENTSTREAM_STATUS, // sends update information regarding a content stream [uint16_t content id] [uint8_t CONTENTSTATUS]
        PKTID_CONTENTSTREAM_CHUNK, // [var-length] [uint16_t content id] [content]
        PKTID_PING,
        PKTID_PONG, // (sent in response to PKTID_PING)
        // ======= CLIENT TO SERVER PACKETS =======
        C2S_HANDSHAKE, // sends info like FoxNet version & endian flag
        // ======= SERVER TO CLIENT PACKETS =======
        S2C_HANDSHAKE, // responds to C2S_HANDSHAKE, tells if the handshake is accepted
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

    inline bool isBigEndian() {
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