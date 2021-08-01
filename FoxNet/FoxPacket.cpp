#include "FoxPacket.hpp"
#include "FoxPeer.hpp"
#include "FoxServer.hpp"

#include <cstring>
#include <iostream>

#define DEF_FOXNET_PACKET(ID) void HANDLER_##ID(ByteStream *stream, FoxPeer *peer, void *udata)
#define INIT_FOXNET_MAP(ID, ptype, sz) {ID, {sz, HANDLER_##ID, ptype}},

using namespace FoxNet;

// ============================================= [[ CLIENT 2 SERVER ]] =============================================

DEF_FOXNET_PACKET(C2S_HANDSHAKE) {
    FoxServer *server = (FoxServer*)udata;
    char magic[FOXMAGICLEN];
    Byte minor, major, endian;
    Byte response;

    stream->readBytes((Byte*)magic, FOXMAGICLEN);
    stream->readByte(major);
    stream->readByte(minor);
    stream->readByte(endian);
    stream->flush();

    // if our endians are different, set the stream to flip the endians!
    stream->setFlipEndian(endian != isBigEndian());

    std::cout << "Got handshake : (" << magic << ") " << (int)major << "." << (int)minor << " endian type : " << (endian ? "BIG" : "LITTLE") << std::endl;
    response = !memcmp(magic, FOXMAGIC, FOXMAGICLEN) && major == FOXNET_MAJOR;

    // now respond
    stream->writeData((PktID)S2C_HANDSHAKE);
    stream->writeBytes((Byte*)magic, FOXMAGICLEN);
    stream->writeByte(response);
    peer->flushSend();
}

// ============================================= [[ SERVER 2 CLIENT ]] =============================================

DEF_FOXNET_PACKET(S2C_HANDSHAKE) {
    char magic[FOXMAGICLEN];
    Byte response;

    stream->readBytes((Byte*)magic, FOXMAGICLEN);
    stream->readByte(response);
    stream->flush();

    std::cout << "got handshake response : " << (response ? "accepted!" : "failed!") << std::endl;
}

const std::map<PktID, PacketInfo> PktMap = {
    INIT_FOXNET_MAP(C2S_HANDSHAKE, PEER_SERVER, (sizeof(Byte) + sizeof(Byte) + sizeof(Byte) + FOXMAGICLEN))
    INIT_FOXNET_MAP(S2C_HANDSHAKE, PEER_CLIENT, (sizeof(Byte) + FOXMAGICLEN))
};

size_t FoxNet::getPacketSize(PktID id) {
    auto iter = PktMap.find(id);
    if (iter == PktMap.end())
        return 0;

    return (*iter).second.pSize;
}

PktHandler FoxNet::getPacketHandler(PktID id) {
    auto iter = PktMap.find(id);
    if (iter == PktMap.end())
        return nullptr;
    
    return (*iter).second.handler;
}

int FoxNet::getPacketType(PktID id) {
    auto iter = PktMap.find(id);
    if (iter == PktMap.end())
        return -1;
    
    return (*iter).second.subscriber;
}