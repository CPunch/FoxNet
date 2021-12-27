#include "FoxClient.hpp"

#include <cstring>

using namespace FoxNet;

FoxClient::FoxClient() {
    // setup packet ids
    INIT_FOXNET_PACKET(S2C_HANDSHAKE, (sizeof(Byte) + FOXMAGICLEN))
}

void FoxClient::connect(std::string ip, std::string port) {
    // connect to ip & port
    FoxSocket::connect(ip, port);

    // set our socket to non-blocking
    setNonBlocking();

    pList.addSock(static_cast<FoxSocket*>(this));

    // connection successful! send handshake
    writeData((PktID)C2S_HANDSHAKE);
    writeBytes((Byte*)FOXMAGIC, FOXMAGICLEN);
    writeByte(FOXNET_MAJOR);
    writeByte(FOXNET_MINOR);
    writeByte(isBigEndian());

    if (!handlePollOut(pList))
        FOXFATAL("couldn't send C2S_HANDSHAKE!")
}

DECLARE_FOXNET_PACKET(S2C_HANDSHAKE, FoxClient) {
    char magic[FOXMAGICLEN];
    Byte response;

    peer->readBytes((Byte*)magic, FOXMAGICLEN);
    peer->readByte(response);

    if (response)
        peer->onReady();
    else
        peer->kill();
}

void FoxClient::pollPeer(int timeout) {
    if (!isAlive()) {
        FOXWARN("pollPeer() called on a dead connection!");
        return;
    }

    std::vector<FoxPollEvent> event = pList.pollList(timeout);

    if (event.size() != 1)
        return;

    // handle events
    if (event[0].pollIn && !handlePollIn(pList))
        goto _FC_kill;

    if (event[0].pollOut && !handlePollOut(pList))
        goto _FC_kill;

    if (!event[0].pollIn && !event[0].pollOut) { // no events? socket error
_FC_kill:
        kill();
        return;
    }
}
