#pragma once

#include "FoxPacket.hpp"

using namespace FoxNet;

#define XORKEY 28

/* 
 *   Obviously, you wouldn't want something this simple 
 * as your 'form of encryption' however this is just to
 * show that it's possible.
 *
 * eg. for example since the handshake string "FOX\71" is known
 * attackers wouldn't even need to dissassemble the binary to
 * look for the xor key, and could just MITM and bruteforce the
 * xor key until they get the correct handshake magic.
*/
inline void xorData(uint8_t *data, size_t sz) {
    for (int i = 0; i < sz; i++) {
        data[i] = data[i] ^ XORKEY;
    }
}

typedef enum {
    C2S_REQ_ADD = PKTID_USER_PACKET_START,
    S2C_NUM_RESPONSE,
} UserPacketIDs;