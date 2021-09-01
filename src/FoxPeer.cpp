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

void FoxPeer::_setupPackets() {
    INIT_FOXNET_PACKET(PKTID_PING, sizeof(int64_t))
    INIT_FOXNET_PACKET(PKTID_PONG, sizeof(int64_t))
    INIT_FOXNET_PACKET(PKTID_CONTENTSTREAM_REQUEST, sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + 32)
    INIT_FOXNET_PACKET(PKTID_CONTENTSTREAM_STATUS, sizeof(uint16_t) + sizeof(uint8_t))
    INIT_FOXNET_VAR_PACKET(PKTID_CONTENTSTREAM_CHUNK)
}

uint16_t FoxPeer::findNextContentID() {
    return contentID++;
}

// ============== CONTENTSTREAM PACKET DECLARATIONS ==============

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

DECLARE_FOXNET_PACKET(PKTID_CONTENTSTREAM_REQUEST, FoxPeer) {
    sha2::sha256_hash hash;
    ContentInfo cIn;
    uint32_t sz;
    uint16_t id;
    uint8_t type;
    uint8_t resp = CS_READY;

    // grab the size
    peer->readInt<uint32_t>(sz);

    if (sz > CONTENTSTREAM_MAX_SIZE) {
        resp = CS_TOOBIG;
        goto _csReqResponse;
    }

    // grab the id
    peer->readInt<uint16_t>(id);

    // grab the type
    peer->readByte(type);

    if (peer->ContentStreams.find(id) != peer->ContentStreams.end()) {
        resp = CS_EXHAUSED_ID;
        goto _csReqResponse;
    }

    // grab the hash
    for (int i = 0; i < 32; i++)
        peer->readByte(hash[i]);

    // setup the content stream
    cIn.hash = hash;
    cIn.file = std::tmpfile();
    cIn.size = sz;
    cIn.processed = 0;
    cIn.type = type;
    cIn.incomming = true;
    peer->ContentStreams[id] = cIn;

_csReqResponse:
    // respond
    peer->writeByte(PKTID_CONTENTSTREAM_STATUS);
    peer->writeInt<uint16_t>(id);
    peer->writeByte(resp);
}

DECLARE_FOXNET_PACKET(PKTID_CONTENTSTREAM_STATUS, FoxPeer) {
    uint16_t id;
    uint8_t status;

    // grab the id
    peer->readInt<uint16_t>(id);

    // grab the status
    peer->readByte(status);

    // first, check we have a content stream matching the id
    auto contentIter = peer->ContentStreams.find(id);
    if (contentIter == peer->ContentStreams.end())
        return; // we don't have a matching content id, ignore the message

    switch(status) {
        case CS_READY: { // they accepted the request to send content, queue the content stream
            if (!(*contentIter).second.incomming) // minor sanity check
                peer->sendContentQueue.insert(id);
            fseek((*contentIter).second.file, 0, SEEK_SET);
            return;
        }
        case CS_CLOSE: { // they requested to close the content stream
            peer->ContentStreams.erase(contentIter);
            peer->sendContentQueue.erase(id);
            return;
        }
        case CS_EXHAUSED_ID: { // they for some reason already have a content stream with that id?
            // try again using a different id
            ContentInfo cachedInfo = (*contentIter).second;
            id = peer->findNextContentID();

            // erase old id & insert new id
            peer->ContentStreams.erase(contentIter);
            peer->sendContentQueue.erase(id);
            peer->ContentStreams[id] = cachedInfo;

            // write packet to stream
            peer->writeByte(PKTID_CONTENTSTREAM_REQUEST);
            peer->writeInt<uint32_t>(cachedInfo.size);
            peer->writeInt<uint16_t>(id);
            peer->writeInt<uint8_t>(cachedInfo.type);

            for (int i = 0; i < 32; i++)
                peer->writeByte(cachedInfo.hash[i]);

            return;
        }
        case CS_INVALID_ID: { // some request we sent had an invalid content id :p
            // just close the stream for now [TODO]
            peer->ContentStreams.erase(contentIter);
            return;
        }
        case CS_FAILED_HASH: { // their hash doesn't match ours
            std::FILE *fp = (*contentIter).second.file;
            uint8_t type = (*contentIter).second.type;

            // reset the stream
            fseek(fp, 0, SEEK_SET);

            // now close our local stream
            peer->ContentStreams.erase(contentIter);
            peer->sendContentQueue.erase(id);

            // now retry
            peer->reqSendContent(fp, type);
            return;
        }
        case CS_TOOBIG: { // requested file size is too big (aka they ran out of space probably)
            // just close the stream for now
            peer->ContentStreams.erase(contentIter);
            peer->sendContentQueue.erase(id);
            return;
        }
        default: {
            // ignore it, its a malformed packet, newer packet protocol, or they're just fuzzing lol
            return;
        }
    }
}

DECLARE_FOXNET_VAR_PACKET(PKTID_CONTENTSTREAM_CHUNK, FoxPeer) {
    uint16_t id;
    size_t sz = varSize - sizeof(uint16_t);
    uint8_t buff[sz];

    // read id
    if (!peer->readInt<uint16_t>(id))
        return;

    // check we actually have an open content stream matching this id
    auto contentIter = peer->ContentStreams.find(id);
    if (contentIter == peer->ContentStreams.end() || !(*contentIter).second.incomming) { // if it doesn't exist, or it's not marked as an incomming stream, abort!
        // respond with an error
        peer->writeByte(PKTID_CONTENTSTREAM_STATUS);
        peer->writeInt<uint16_t>(id);
        peer->writeByte(CS_INVALID_ID);
        return;
    }

    // read the rest of the stream
    peer->readBytes(buff, sz);

    // write buffer to file
    if (fwrite((void*)buff, sizeof(uint8_t), sz, (*contentIter).second.file) != sz) {
        // error occured while writing, for now just close it
        peer->writeByte(PKTID_CONTENTSTREAM_STATUS);
        peer->writeInt<uint16_t>(id);
        peer->writeByte(CS_CLOSE);
        peer->ContentStreams.erase(contentIter);
        return;
    }

    // update processed bytes
    (*contentIter).second.processed += sz;

    // if we've received the whole content
    if ((*contentIter).second.processed >= (*contentIter).second.size) {
        // grab hash
        fseek((*contentIter).second.file, 0, SEEK_SET);
        sha2::sha256_hash hash = sha2::sha256_file((*contentIter).second.file, (*contentIter).second.size);

        // compare hash
        for (int i = 0; i < 32; i++) {
            if (hash[i] != (*contentIter).second.hash[i]) {
                // respond with an error
                peer->writeByte(PKTID_CONTENTSTREAM_STATUS);
                peer->writeInt<uint16_t>(id);
                peer->writeByte(CS_FAILED_HASH);
                peer->ContentStreams.erase(contentIter);
                return;
            }
        }

        // reset stream, pass it to the event
        fseek((*contentIter).second.file, 0, SEEK_SET);
        peer->onContentReceived((*contentIter).second);

        // erase content stream
        peer->ContentStreams.erase(contentIter);
    }
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
    uint16_t pSize = sizeOut() - sizeof(uint16_t) - sizeof(uint8_t) - indx;

    // now patch the dummy size, (first 2 bytes)
    patchInt(pSize, indx);
}

void FoxPeer::reqSendContent(std::FILE *file, uint8_t contentType) {
    ContentInfo cOut;
    sha2::sha256_hash hash;
    uint16_t id = findNextContentID();
    size_t sz;

    // grab the file size
    fseek(file, 0L, SEEK_END);
    sz = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (sz > CONTENTSTREAM_MAX_SIZE) {
        FOXFATAL("File stream is too large! aborting content send..");
    }

    // generate the file hash
    hash = sha2::sha256_file(file, sz);
    fseek(file, 0, SEEK_SET);

    writeByte(PKTID_CONTENTSTREAM_REQUEST);

    // write the buffer size
    writeInt<uint32_t>(sz);

    // write the id
    writeInt<uint16_t>(id);

    // write the content type
    writeInt<uint8_t>(contentType);

    // write hash to buffer
    for (int i = 0; i < 32; i++)
        writeByte(hash[i]);

    cOut.hash = hash;
    cOut.processed = 0;
    cOut.size = sz;
    cOut.file = file;
    cOut.type = contentType;
    cOut.incomming = false;
    ContentStreams[id] = cOut;
}

bool FoxPeer::sendContentChunk(uint16_t id) {
    auto contIter = ContentStreams.find(id);
    if (contIter == ContentStreams.end()) // sanity check
        return false;

    size_t indx = prepareVarPacket(PKTID_CONTENTSTREAM_CHUNK);
    size_t sz = (*contIter).second.size - (*contIter).second.processed;
    int read;

    // make sure we don't overflow the packet size
    if (sz > MAX_PACKET_SIZE - sizeof(uint16_t)) // uint16_t is to make sure we have room for our content id
        sz = MAX_PACKET_SIZE - sizeof(uint16_t);

    uint8_t buf[sz];

    // write the content id
    writeInt<uint16_t>(id);

    // read from the file into our temp buff
    if ((read = fread((void*)buf, sizeof(uint8_t), sz, (*contIter).second.file)) != sz) {
        // just fail silently, erase the content stream (but not from the queue since this can be called while iterating over it!)
        ContentStreams.erase(contIter);
        return true;
    }

    // write buffer to out stream
    writeBytes(buf, sz);

    // complete var packet
    patchVarPacket(indx);

    // end stream if we're done
    if (((*contIter).second.processed += sz) == (*contIter).second.size) {
        onContentSent((*contIter).second);
        ContentStreams.erase(contIter);
    }

    return true;
}

bool FoxPeer::flushSend() {
    std::vector<Byte> buffer = getOutBuffer();
    size_t sentBytes = 0;
    int sent;

    // sanity check, don't send nothing
    if (buffer.size() == 0)
        return true;

    //std::cout << "sending " << buffer.size() << " bytes..." << std::endl;

    // write bytes to the socket until an error occurs or we finish reading
    do {
        sent = write(sock, (buffer_t*)(&buffer[sentBytes]), buffer.size() - sentBytes);
        /*std::cout << " . ";
        for (size_t i = 0; i < sent; i++) {
            std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i+sentBytes] << std::dec << " ";
            if ((i+1) % 8 == 0)
                std::cout << std::endl;
        }*/

        // if the socket closed or an error occured, return the error result
        if (sent == 0 || (SOCKETERROR(sent) && FN_ERRNO != FN_EWOULD))
            return false;
    } while((sentBytes += sent) != buffer.size());

    //std::cout << std::endl;

    flushOut();
    return true;
}

int FoxPeer::rawRecv(size_t sz) {
    if (sz == 0) // sanity check
        return 0;

    Byte buf[sz];
    int rcvd = read(sock, (buffer_t*)(buf), sz);

    /*std::cout << "recieved " << rcvd << " bytes" << std::endl;

    for (int i = 0; i < rcvd; i++) {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << std::dec << " ";
        if ((i+1) % 8 == 0)
            std::cout << std::endl;
    }

    std::cout << std::endl;*/

    if (rcvd == 0 || (SOCKETERROR(rcvd) && FN_ERRNO != FN_EWOULD
#ifndef _WIN32
        // if it's a posix system, also make sure its not a EAGAIN result (which is a recoverable error, there's just nothing to read lol)
        && FN_ERRNO != EAGAIN
#endif
        ))
    // if the socket closed or an error occured, return the error result
        return -1;

    if (rcvd > 0)
        rawWriteIn(buf, rcvd);
    else
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

bool FoxPeer::onContentRequest(uint8_t type, const ContentInfo content) {
    return false; // by default reject every request if the user doesn't implement this method
}

void FoxPeer::onContentReceived(const ContentInfo content) {
    // stubbed
}

void FoxPeer::onContentSent(const ContentInfo content) {
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

    // fire our event
    onKilled();
}

bool FoxPeer::sendStep() {
    ContentInfo info;

    for (auto iter = sendContentQueue.begin(); iter != sendContentQueue.end();) {
        auto contIter = ContentStreams.find(*iter);
        if (contIter == ContentStreams.end()) { // if it doesn't exist in ContentStreams, erase it!
            iter = sendContentQueue.erase(iter);
            continue;
        }

        info = (*contIter).second;

        if (!info.incomming && info.processed < info.size && !sendContentChunk(*iter)) {
            return false; // sendContentChunk() failed, which means a broken socket
        }

        iter++;
    }

    if (sizeOut() > 0 && !flushSend()) {
        return false;
    }

    onStep();

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
            int expectedRec = pktSize - sizeIn();

            // try to get our packet, if we error return false
            if (expectedRec != 0 && (rec = rawRecv(expectedRec)) == -1)
                return false;

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
                flushIn(); // just make sure we don't leave any unused bytes in the queue so we don't mess up future packets
                currentPkt = PKTID_NONE;
                break;
            }

            break;
        }
    }

    return true;
}