#pragma once

// socket/winsock headers
#ifdef _WIN32
// windows
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    typedef char buffer_t;
    #define PollFD WSAPOLLFD
    #define poll WSAPoll
    #define FN_ERRNO WSAGetLastError()
    #define FN_EWOULD WSAEWOULDBLOCK
    #define SOCKETINVALID(x) (x == INVALID_SOCKET)
    #define SOCKETERROR(x) (x == SOCKET_ERROR)
#else
// posix platform
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <poll.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int SOCKET;
    typedef void buffer_t;
    #define PollFD struct pollfd
    #define FN_ERRNO errno
    #define FN_EWOULD EWOULDBLOCK
    #define SOCKETINVALID(x) (x < 0)
    #define SOCKETERROR(x) (x == -1)
#endif
#include <fcntl.h>
#include <cstdio>
#include <map>
#include <unordered_set>

#include "ByteStream.hpp"
#include "FoxPacket.hpp"
#include "SHA2.hpp"

// 1gb
#define CONTENTSTREAM_MAX_SIZE 1073741824
#define FOXNET_PACKET_HANDLER(ID) HANDLER_##ID

#define DEF_FOXNET_PACKET(ID) static void FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer);
#define DECLARE_FOXNET_PACKET(ID, className) void className::FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer)
#define INIT_FOXNET_PACKET(ID, sz) PKTMAP[ID].handler = FOXNET_PACKET_HANDLER(ID); PKTMAP[ID].size = sz; PKTMAP[ID].variable = false;

#define DEF_FOXNET_VAR_PACKET(ID) static void FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer, PktSize varSize);
#define DECLARE_FOXNET_VAR_PACKET(ID, className) void className::FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer, PktSize varSize)
#define INIT_FOXNET_VAR_PACKET(ID) PKTMAP[ID].varhandler = FOXNET_PACKET_HANDLER(ID); PKTMAP[ID].size = 0; PKTMAP[ID].variable = true;

namespace FoxNet {
    bool setSockNonblocking(SOCKET sock);
    void killSocket(SOCKET sock);

    typedef enum {
        CS_READY, // peer is ready to receive the content
        CS_CLOSE, // content stream
        // error results
        CS_EXHAUSED_ID, // sent content stream id is already in use
        CS_INVALID_ID, // sent content stream id doesn't exist
        CS_FAILED_HASH, // sent content stream doesn't match the sent hash
        CS_TOOBIG  // requested content stream size is too big (>1gb)
    } CONTENTSTATUS;

    /*
    * This struct holds information on content streams
    */
    struct ContentInfo {
        sha2::sha256_hash hash;
        std::FILE *file; // temporary file handle, as the content is recevied/sent it is written to/read from this temporary file
        size_t processed;
        size_t size;
        uint8_t type;
        bool incomming; // is this being recieved or sent?
    };

    class FoxPeer : public ByteStream {
    private:
        PktID currentPkt = PKTID_NONE;
        PktSize pktSize;
        bool alive = true;
        uint16_t contentID = 0;

        DEF_FOXNET_PACKET(PKTID_PING)
        DEF_FOXNET_PACKET(PKTID_PONG)
        DEF_FOXNET_PACKET(PKTID_CONTENTSTREAM_REQUEST)
        DEF_FOXNET_PACKET(PKTID_CONTENTSTREAM_STATUS)
        DEF_FOXNET_VAR_PACKET(PKTID_CONTENTSTREAM_CHUNK)

        void _setupPackets();
        uint16_t findNextContentID();

        std::map<uint16_t, ContentInfo> ContentStreams;
        std::unordered_set<uint16_t> sendContentQueue;

    protected:
        PacketInfo PKTMAP[UINT8_MAX+1];
        SOCKET sock;

        int rawRecv(size_t sz);

        bool sendContentChunk(uint16_t id);

        bool isPacketVar(PktID);
        PktSize getPacketSize(PktID);
        PktHandler getPacketHandler(PktID);
        PktVarHandler getVarPacketHandler(PktID);

    public:
        FoxPeer();
        FoxPeer(SOCKET);
        ~FoxPeer();

        /*
         * This should be called prior to writing packet data to the stream.
         * returns: start index of the var packet, pass this result to patchVarPacket()
         */
        size_t prepareVarPacket(PktID id);

        /* 
         * After writing your variable-length packet to the stream, call this and it will patch the length.
         * make sure you call this BEFORE SENDING YOUR PACKET! remember, flushSend() is called after each packet
         * handler if there is data in the stream to send!
         */
        void patchVarPacket(size_t indx);

        // events
        virtual void onReady(); // fired when we got a handshake response from the server and it went well :)
        virtual void onKilled(); // fired when we have been killed (peer disconnect)
        virtual void onStep(); // fired when sendStep() is called
        virtual void onPing(int64_t peerTime, int64_t currTime); // fired when PKTID_PING is received
        virtual void onPong(int64_t peerTime, int64_t currTime); // fired when PKTID_PONG is received
        virtual bool onContentRequest(uint8_t type, const ContentInfo content); // fired whenever a PKTID_CONTENTSTREAM_REQUEST is received, if this returns true the content is accepted
        virtual void onContentReceived(const ContentInfo content); // fired after the whole content was received
        virtual void onContentSent(const ContentInfo content); // fired after content has completed being sent

        void reqSendContent(std::FILE *file, uint8_t contentType);

        bool isAlive();

        void kill();
        bool recvStep(); // only call this when poll() returns an event on this socket
        bool sendStep(); // call this before calling poll()
        bool flushSend();
    };
}