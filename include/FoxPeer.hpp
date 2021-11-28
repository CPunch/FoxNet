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
#ifdef __linux__
    #include <sys/epoll.h>
    // max events for epoll()
    #define MAX_EPOLL_EVENTS 128
#endif
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
#include "FoxException.hpp"


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

    void _FoxNet_Init();
    void _FoxNet_Cleanup();

    class FoxPeer : public ByteStream {
    private:
        PktID currentPkt = PKTID_NONE;
        PktSize pktSize;
        bool alive = true;

        DEF_FOXNET_PACKET(PKTID_PING)
        DEF_FOXNET_PACKET(PKTID_PONG)

        void _setupPackets();

    protected:
        PacketInfo PKTMAP[UINT8_MAX+1];
        FoxException cachedException;
        bool exceptionThrown = false;
        SOCKET sock;

        int rawRecv(size_t sz);

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
        virtual void onSend(uint8_t *data, size_t sz); // fired right before data is sent over the socket
        virtual void onRecv(uint8_t *data, size_t sz); // fired right after data was received from the socket

        bool isAlive();

        void kill();
        bool recvStep(); // only call this when poll() returns an event on this socket
        bool sendStep(); // call this before calling poll()
        bool flushSend();

        SOCKET getRawSock();
    };
}