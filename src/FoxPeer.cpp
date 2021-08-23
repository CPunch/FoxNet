#include "FoxPeer.hpp"
#include "FoxPacket.hpp"

#include <iostream>
#include <iomanip>

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

using namespace FoxNet;

FoxPeer::FoxPeer() {
    for (int i = 0; i < UINT8_MAX; i++)
        PKTMAP[i] = PacketInfo(nullptr, 0);
}

FoxPeer::FoxPeer(SOCKET _sock) {
    sock = _sock;

    for (int i = 0; i < UINT8_MAX; i++)
        PKTMAP[i] = PacketInfo(nullptr, 0);
}

void FoxPeer::prepareVarPacket(PktID id) {
    uint16_t dummySize = 0;

    // write our dummy size, this'll be overwritten by patchVarPacket
    writeUInt(dummySize);

    // then write our packet id
    writeByte(id);
}

void FoxPeer::patchVarPacket() {
    // get the size of the packet
    uint16_t pSize = size() - sizeof(uint32_t) - sizeof(uint8_t);

    // now patch the dummy size, (first 2 bytes)
    patchUInt(pSize, 0);
}

bool FoxPeer::flushSend() {
    std::vector<Byte> buffer = getBuffer();
    size_t sentBytes = 0;
    size_t sent;

    // sanity check, don't send nothing
    if (buffer.size() == 0)
        return true;

    /*std::cout << "sending " << buffer.size() << " bytes..." << std::endl;

    for (size_t i = 0; i < buffer.size(); i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i] << std::dec << " ";
        if ((i+1) % 8 == 0)
            std::cout << std::endl;
    }

    std::cout << std::endl;*/

    // write bytes to the socket until an error occurs or we finish reading
    do {
        sent = write(sock, (buffer_t*)(&buffer[sentBytes]), buffer.size() - sentBytes);

        // if the socket closed or an error occured, return the error result
        if (sent == 0 || (SOCKETERROR(sent) && FN_ERRNO != FN_EWOULD))
            return false;
    } while((sentBytes += sent) != buffer.size());

    flush();
    return true;
}

int FoxPeer::rawRecv(size_t sz) {
    Byte buf[sz];
    size_t rcvd = read(sock, (buffer_t*)(buf), sz);

    /*for (size_t i = 0; i < sz; i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << std::dec << " ";
        if ((i+1) % 8 == 0)
            std::cout << std::endl;
    }*/

    std::cout << std::endl;


    // if the socket closed or an error occured, return the error result
    if (rcvd == 0 || (SOCKETERROR(rcvd) && FN_ERRNO != FN_EWOULD))
        return 0;

    writeBytes(buf, rcvd);
    return rcvd;
}

PktHandler FoxPeer::getPacketHandler(PktID id) {
    return PKTMAP[id].handler;
}

PktSize FoxPeer::getPacketSize(PktID id) {
    return PKTMAP[id].pSize;
}

void FoxPeer::onReady() {
    // stubbed
}

bool FoxPeer::isAlive() {
    return alive;
}

void FoxPeer::kill() {
    if (!alive)
        return;

    alive = false;
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
    closesocket(sock);
#else
    shutdown(sock, SHUT_RDWR);
    close(sock);
#endif
}

bool FoxPeer::step() {
    switch(currentPkt) {
        case PKTID_NONE: // we're queued to receive a packet
            if (rawRecv(sizeof(PktID)) != sizeof(PktID))
                return false;

            readByte(currentPkt);
            pktSize = getPacketSize(currentPkt);
            break;
        case PKTID_VAR_LENGTH: {
            if (pktSize == 0) {
                PktSize pSize;
                // grab packet length
                if (rawRecv(sizeof(PktSize)) != sizeof(PktSize))
                    return false;

                readUInt(pSize);
                pktSize = pSize;

                // if they try sending a packet larger than MAX_PACKET_SIZE kill em'
                if (pktSize > MAX_PACKET_SIZE)
                    return false;
            } else {
                // after we have our size, the pkt ID is next
                if (rawRecv(sizeof(PktID)) != sizeof(PktID))
                    return false;

                readByte(currentPkt);
            }
            break;
        }
        default: {
            int rec;
            int expectedRec = pktSize - buffer.size();

            // try to get our packet, if we fail return false
            if (expectedRec != 0 && (rec = rawRecv(expectedRec)) == 0)
                return false;

            if (pktSize == buffer.size()) {
                // dispatch Packet Handler
                PktHandler hndlr = getPacketHandler(currentPkt);
                if (hndlr != nullptr) {
                    hndlr(this);
                    flushSend();
                } else {
                    flush(); // we'll just ignore the packet
                }

                // reset
                currentPkt = PKTID_NONE;
                break;
            }

            break;
        }
    }

    return true;
}