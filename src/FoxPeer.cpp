#include "FoxPeer.hpp"
#include "FoxPacket.hpp"

#include <iostream>
#include <iomanip>

using namespace FoxNet;

DECLARE_FOXNET_PACKET(PKTID_PING, FoxPeer) {
    int64_t peerTime;
    int64_t currTime = getTimestamp();

    peer->readInt<int64_t>(peerTime);

    peer->writeByte(PKTID_PONG);
    peer->writeInt<int64_t>(currTime);

    peer->onPing(peerTime, currTime);
}

DECLARE_FOXNET_PACKET(PKTID_PONG, FoxPeer) {
    int64_t peerTime;
    int64_t currTime = getTimestamp();

    peer->readInt<int64_t>(peerTime);

    peer->onPong(peerTime, currTime);
}

FoxPeer::FoxPeer() {
    for (int i = 0; i < UINT8_MAX; i++)
        PKTMAP[i] = PacketInfo();

    INIT_FOXNET_PACKET(PKTID_PING, sizeof(int64_t))
    INIT_FOXNET_PACKET(PKTID_PONG, sizeof(int64_t))
}

size_t FoxPeer::prepareVarPacket(PktID id) {
    uint16_t dummySize = 0;

    writeByte(PKTID_VAR_LENGTH);

    size_t indx = sizeOut();

    // write our dummy size, this'll be overwritten by patchVarPacket
    writeInt<uint16_t>(dummySize);

    // then write our packet id
    writeByte(id);

    return indx;
}

void FoxPeer::patchVarPacket(size_t indx) {
    // get the size of the packet
    uint16_t pSize = (uint16_t)(sizeOut() - sizeof(uint16_t) - sizeof(uint8_t) - indx);

    // now patch the dummy size, (first 2 bytes)
    patchInt(pSize, indx);
}

bool FoxPeer::isPacketVar(PktID id) {
    return PKTMAP[id].variable;
}

PktHandler FoxPeer::getPacketHandler(PktID id) {
    return PKTMAP[id].handler;
}

PktVarHandler FoxPeer::getVarPacketHandler(PktID id) {
    return PKTMAP[id].varhandler;
}

PktSize FoxPeer::getPacketSize(PktID id) {
    return PKTMAP[id].size;
}

void FoxPeer::onReady() {
    // stubbed
}

void FoxPeer::onStep() {
    // stubbed
}

void FoxPeer::onPing(int64_t peerTime, int64_t currTime) {
    // stubbed
}

void FoxPeer::onPong(int64_t peerTime, int64_t currTime) {
    // stubbed
}

bool FoxPeer::handlePollIn(FoxPollList& plist) {
    RawSockReturn recv;
    PktSize pSize;

    switch(currentPkt) {
        case PKTID_NONE: // we're queued to receive a packet
            recv = rawRecv(sizeof(PktID));

            switch (recv.code) {
                case RAWSOCK_OK:
                    readByte(currentPkt);
                    pktSize = getPacketSize(currentPkt);
                    break;
                case RAWSOCK_CLOSED:
                case RAWSOCK_ERROR:
                default: // ??
                    return false;
            }
            break;
        case PKTID_VAR_LENGTH:
            if (pktSize == 0) {
                // grab packet length
                recv = rawRecv(sizeof(PktSize));

                switch (recv.code) {
                    case RAWSOCK_OK:
                        readInt<uint16_t>(pSize);
                        pktSize = pSize;

                        // if they try sending a packet larger than MAX_PACKET_SIZE kill em'
                        if (pktSize > MAX_PACKET_SIZE)
                            return false;
                        break;
                    case RAWSOCK_CLOSED:
                    case RAWSOCK_ERROR:
                    default: // ??
                        return false;
                }
            } else {
                recv = rawRecv(sizeof(PktID));

                switch (recv.code) {
                    case RAWSOCK_OK:
                        readByte(currentPkt);
                        break;
                    case RAWSOCK_CLOSED:
                    case RAWSOCK_ERROR:
                    default: // ??
                        return false;
                }
            }
            break;
        default: {
            int rec;
            int expectedRec = (int)(pktSize - sizeIn());

            // try to get our packet, if we error return false
            if (expectedRec != 0) {
                recv = rawRecv(expectedRec);

                switch (recv.code) {
                    case RAWSOCK_OK:
                        break;
                    case RAWSOCK_CLOSED:
                    case RAWSOCK_ERROR:
                    default: // ??
                        return false;
                }
            }

            if (sizeIn() == pktSize) {
                if (isPacketVar(currentPkt)) {
                    PktVarHandler hndlr = getVarPacketHandler(currentPkt);
                    if (hndlr != nullptr) {
                        hndlr(this, pktSize);
                    }
                } else {
                    // dispatch Packet Handler
                    PktHandler hndlr = getPacketHandler(currentPkt);
                    if (hndlr != nullptr) {
                        hndlr(this);
                    }
                }

                // reset
                flushIn(); // just make sure we don't leave any unused bytes in the queue so we don't mess up future received packets
                currentPkt = PKTID_NONE;
                break;
            }

            break;
        }
    }

    // we have data to send and handePollOut returns an error, return error result
    if (sizeOut() > 0 && !handlePollOut(plist))
        return false;

    return isAlive();
}

bool FoxPeer::handlePollOut(FoxPollList& plist) {
    RawSockReturn sent;

    // sanity check
    if (sizeOut() == 0)
        return true;

    onStep();
    sent = rawSend(sizeOut());

    switch(sent.code) {
        case RAWSOCK_OK: // we're ok!
            if (setPollOut) { // if POLLOUT was set, unset it
                plist.rmvPollOut(this);
                setPollOut = false;
            }
            return true;
        case RAWSOCK_POLL: // we've been asked to set the POLLOUT flag
            if (!setPollOut) { // if POLLOUT wasn't set, set it so we'll be notified whenever the kernel has room :)
                plist.addPollOut(this);
                setPollOut = true;
            }
            return true;
        default:
        case RAWSOCK_CLOSED:
        case RAWSOCK_ERROR:
            return false;
    }
}

SOCKET FoxPeer::getRawSock() {
    return sock;
}