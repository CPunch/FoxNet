#include "FoxPeer.hpp"
#include "FoxPacket.hpp"

#include <iostream>
#include <iomanip>
#include <mutex>

// if _FNSetup > 0, WSA has already been started. if _FNSetup == 0, WSA needs to be cleaned up
static int _FNSetup = 0;
static std::mutex _FNSetupLock;

bool FoxNet::setSockNonblocking(SOCKET sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
    if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
        FOXWARN("fcntl failed on new connection");
#ifdef _WIN32
        shutdown(sock, SD_BOTH);
        closesocket(sock);
#else
        shutdown(sock, SHUT_RDWR);
        close(sock);
#endif
        return false;
    }

    return true;
}

void FoxNet::killSocket(SOCKET sock) {
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
    closesocket(sock);
#else
    shutdown(sock, SHUT_RDWR);
    close(sock);
#endif
}

// this *SHOULD* be called before any socket API. On POSIX platforms this is stubbed, however on Windows this is required to start WSA
void FoxNet::_FoxNet_Init() {
    std::lock_guard<std::mutex> FNLock(_FNSetupLock);

    if (_FNSetup++ > 0)
        return; // WSA is already setup!

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        FOXFATAL("WSAStartup failed!")
    }
#endif
}

// this *SHOULD* only be called when there is no more socket API to be called. On POSIX platforms this is stubbed, however on Windows this is required to cleanup WSA
void FoxNet::_FoxNet_Cleanup() {
    std::lock_guard<std::mutex> FNLock(_FNSetupLock);

    if (--_FNSetup > 0)
        return; // WSA still needs to be up, a FoxNet peer is still using it

#ifdef _WIN32
    WSACleanup();
#endif
}

using namespace FoxNet;

// this function is called first before any derived class is constructed, so it's the perfect place to initialize 
// our default packet map & initialize WSA (if we're on windows)
void FoxPeer::_setupPackets() {
    INIT_FOXNET_PACKET(PKTID_PING, sizeof(int64_t))
    INIT_FOXNET_PACKET(PKTID_PONG, sizeof(int64_t))

    _FoxNet_Init();
}

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

    _setupPackets();
}

FoxPeer::FoxPeer(SOCKET _sock) {
    sock = _sock;

    for (int i = 0; i < UINT8_MAX; i++)
        PKTMAP[i] = PacketInfo();

    _setupPackets();
}

// the base class is deconstructed last, so it's the perfect place to cleanup WSA (if we're on windows)
FoxPeer::~FoxPeer() {
    _FoxNet_Cleanup();
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

int FoxPeer::rawRecv(size_t sz) {
    if (sz == 0) // sanity check
        return 0;

    VLA<Byte> buf(sz);
    int rcvd = recv(sock, (buffer_t*)(buf.buf), sz, 0);

#ifdef DEBUG
    std::cout << "received " << rcvd << " bytes { " << std::endl << "\t";

    for (int i = 0; i < rcvd; i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << std::dec << " ";
        if ((i+1) % 8 == 0)
            std::cout << std::endl << "\t";
    }

    std::cout << std::endl << "}" << std::endl;
#endif

    if (rcvd == 0 || (SOCKETERROR(rcvd) && FN_ERRNO != FN_EWOULD
#ifndef _WIN32
        // if it's a posix system, also make sure its not a EAGAIN result (which is a recoverable error, there's just nothing to read lol)
        && FN_ERRNO != EAGAIN
#endif
        )) {
        // if the socket closed or an error occurred, return the error result
        return -1;
    }

    if (rcvd > 0) {
        // call our event, they'll modify the data (probably)
        onRecv(buf.buf, rcvd);
        rawWriteIn(buf.buf, rcvd);
    } else
        return 0;

    return rcvd;
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

void FoxPeer::onKilled() {
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

void FoxPeer::onSend(uint8_t *data, size_t sz) {
    // stubbed
}

void FoxPeer::onRecv(uint8_t *data, size_t sz) {
    // stubbed
}

bool FoxPeer::isAlive() {
    return alive;
}

void FoxPeer::kill() {
    if (!alive)
        return;

    alive = false;
    killSocket(sock);

    // fire our event
    onKilled();
}

bool FoxPeer::sendStep() {
    // call onStep() before flushing our send buffer so that any queued bytes will be sent
    onStep();

    if (sizeOut() > 0 && !flushSend()) {
        return false;
    }

    return true;
}

bool FoxPeer::recvStep() {
    int recv;
    switch(currentPkt) {
        case PKTID_NONE: // we're queued to receive a packet
            if ((recv = rawRecv(sizeof(PktID))) == -1) // -1 is the "actual error" result
                return false;

            // its a recoverable error, so if we didn't get what we expected, just try again!
            //  (^ motto for life honestly)
            if (recv != sizeof(PktID))
                return true;

            readByte(currentPkt);
            pktSize = getPacketSize(currentPkt);
            break;
        case PKTID_VAR_LENGTH: {
            if (pktSize == 0) {
                PktSize pSize;
                // grab packet length
                if ((recv = rawRecv(sizeof(PktSize) - sizeIn())) == -1)
                    return false;

                if (sizeIn() != sizeof(PktSize)) // try again
                    return true;

                readInt<uint16_t>(pSize);
                pktSize = pSize;

                // if they try sending a packet larger than MAX_PACKET_SIZE kill em'
                if (pktSize > MAX_PACKET_SIZE)
                    return false;
            } else {
                // after we have our size, the pkt ID is next
                if ((recv = rawRecv(sizeof(PktID))) == -1)
                    return false;

                if (recv != sizeof(PktID)) // try again
                    return true;

                readByte(currentPkt);
            }
            break;
        }
        default: {
            int rec;
            int expectedRec = (int)(pktSize - sizeIn());

            // try to get our packet, if we error return false
            if (expectedRec != 0 && (rec = rawRecv(expectedRec)) == -1)
                return false;

            if (sizeIn() == pktSize) {
                try {
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
                } catch(FoxException &x) {
                    // report the exception, the caller will decide to ignore it or not
                    cachedException = x;
                    exceptionThrown = true;
                    return false;
                }

                // reset
                flushIn(); // just make sure we don't leave any unused bytes in the queue so we don't mess up future received packets
                currentPkt = PKTID_NONE;
                break;
            }

            break;
        }
    }

    return isAlive();
}

bool FoxPeer::flushSend() {
    std::vector<Byte> buffer = getOutBuffer();
    size_t sentBytes = 0;
    int sent;

    // sanity check, don't send nothing
    if (buffer.size() == 0)
        return true;

    // call our event, they'll modify the data (probably)
    onSend(buffer.data(), buffer.size());

    // write bytes to the socket until an error occurs or we finish reading
    do {
        sent = send(sock, (buffer_t*)(&buffer[sentBytes]), buffer.size() - sentBytes, 0);

        // if the socket closed or an error occurred, return the error result
        if (sent == 0 || (SOCKETERROR(sent) && FN_ERRNO != FN_EWOULD))
            return false;
    } while((sentBytes += sent) != buffer.size());

#ifdef DEBUG
    std::cout << "sent " << sentBytes << " bytes : { " << std::endl << "\t";

    for (size_t i = 0; i < sentBytes; i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i] << std::dec << " ";
        if ((i+1) % 8 == 0)
            std::cout << std::endl << "\t";
    }

    std::cout << std::endl << "}" << std::endl;
#endif

    flushOut();
    return true;
}

SOCKET FoxPeer::getRawSock() {
    return sock;
}