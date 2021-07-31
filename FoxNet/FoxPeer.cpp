#include "FoxPeer.hpp"
#include "FoxPacket.hpp"

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

FoxPeer::FoxPeer() {}
FoxPeer::FoxPeer(SOCKET _sock) {
    sock = _sock;
}

bool FoxPeer::flushSend() {
    std::vector<Byte> buffer = stream.getBuffer();
    int sentBytes = 0;
    int sent;

    // write bytes to the socket until an error occurs or we finish reading
    do {
        sent = write(sock, (buffer_t*)(&buffer[sentBytes]), buffer.size() - sentBytes);

        // if the socket closed or an error occured, return the error result
        if (sent == 0 || (SOCKETERROR(sent) && FN_ERRNO != FN_EWOULD))
            return false;
    } while((sentBytes += sent) != buffer.size());

    stream.flush();
    return true;
}

int FoxPeer::rawRecv(size_t sz) {
    Byte buf[sz];
    int rcvd = read(sock, (buffer_t*)(buf), sz);

    // if the socket closed or an error occured, return the error result
    if (rcvd == 0 || (SOCKETERROR(rcvd) && FN_ERRNO != FN_EWOULD))
        return 0;

    stream.writeBytes(buf, rcvd);
    return rcvd;
}

bool FoxPeer::isAlive() {
    return alive;
}

void FoxPeer::setHndlerUData(void* udata) {
    userdata = udata;
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
            if (rawRecv(sizeof(PktID)) == 0)
                return false;

            stream.readData(currentPkt);
            pktSize = getPacketSize(currentPkt);
            break;
        default: {
            // try to get our packet, if we fail return false
            if (rawRecv(pktSize - stream.size()) == 0)
                return false;

            if (stream.size() == pktSize) {
                // dispatch Packet Handler
                PktHandler hndlr = getPacketHandler(currentPkt);
                if (hndlr != nullptr && getPacketType(currentPkt) == type)
                    hndlr(&stream, this, userdata);

                // reset
                stream.flush();
                currentPkt = PKTID_NONE;
            }
            break;
        }
    }

    return true;
}