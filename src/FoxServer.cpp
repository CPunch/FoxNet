#include "FoxServer.hpp"
#include "FoxPacket.hpp"

using namespace FoxNet;

FoxServerPeer::FoxServerPeer() {
    setupPackets();
}

FoxServerPeer::FoxServerPeer(SOCKET sock): FoxPeer(sock) {
    setupPackets();
}

void FoxServerPeer::setupPackets() {
    INIT_FOXNET_PACKET(C2S_HANDSHAKE, (sizeof(Byte) + sizeof(Byte) + sizeof(Byte) + FOXMAGICLEN))
}

DECLARE_FOXNET_PACKET(C2S_HANDSHAKE, FoxServerPeer) {
    FoxServerPeer *sPeer = dynamic_cast<FoxServerPeer*>(peer);
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

    peer->onReady();
} 