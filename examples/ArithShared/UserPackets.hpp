#pragma once

#include "FoxPacket.hpp"

using namespace FoxNet;

typedef enum {
    C2S_REQ_ADD = PKTID_USER_PACKET_START,
    S2C_NUM_RESPONSE,
} UserPacketIDs;