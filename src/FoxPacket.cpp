#include "FoxPacket.hpp"
#include "FoxPeer.hpp"
#include "FoxServer.hpp"

#include <cstring>
#include <iostream>

#define DEF_FOXNET_PACKET(ID) void HANDLER_##ID(FoxPeer *peer, void *udata)
#define INIT_FOXNET_MAP(ID, ptype, sz) {ID, {sz, HANDLER_##ID, ptype}},

using namespace FoxNet;

// ============================================= [[ CLIENT 2 SERVER ]] =============================================

DEF_FOXNET_PACKET(C2S_HANDSHAKE) {
    //FoxServer *server = (FoxServer*)udata;
    char magic[FOXMAGICLEN];
    Byte minor, major, endian;
    Byte response;

    peer->readBytes((Byte*)magic, FOXMAGICLEN);
    peer->readByte(major);
    peer->readByte(minor);
    peer->readByte(endian);
    peer->flush();

    // if our endians are different, set the peer to flip the endians!
    peer->setFlipEndian(endian != isBigEndian());

    std::cout << "Got handshake : (" << magic << ") " << (int)major << "." << (int)minor << " flip endian : " << (endian != isBigEndian() ? "TRUE" : "FALSE") << std::endl;
    response = !memcmp(magic, FOXMAGIC, FOXMAGICLEN) && major == FOXNET_MAJOR;

    // now respond
    peer->writeByte(S2C_HANDSHAKE);
    peer->writeBytes((Byte*)magic, FOXMAGICLEN);
    peer->writeByte(response);
}

// ============================================= [[ SERVER 2 CLIENT ]] =============================================

DEF_FOXNET_PACKET(S2C_HANDSHAKE) {
    char magic[FOXMAGICLEN];
    Byte response;

    peer->readBytes((Byte*)magic, FOXMAGICLEN);
    peer->readByte(response);
    peer->flush();

    std::cout << "got handshake response : " << (response ? "accepted!" : "failed!") << std::endl;

    peer->callEvent(PEEREVENT_ONREADY);
}

std::map<PktID, PacketInfo> PktMap = {
    INIT_FOXNET_MAP(C2S_HANDSHAKE, PEER_SERVER, (sizeof(Byte) + sizeof(Byte) + sizeof(Byte) + FOXMAGICLEN))
    INIT_FOXNET_MAP(S2C_HANDSHAKE, PEER_CLIENT, (sizeof(Byte) + FOXMAGICLEN))
};

void FoxNet::registerUserPacket(PktID uid, PktHandler handler, int subscriber, size_t pktsize) {
    PktMap[uid] = {pktsize, handler, subscriber};
}

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