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

#include "ByteStream.hpp"
#include "FoxPacket.hpp"


#define FOXNET_PACKET_HANDLER(ID) HANDLER_##ID
#define DEF_FOXNET_PACKET(ID) static void FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer);
#define DECLARE_FOXNET_PACKET(ID, className) void className::FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer)
#define INIT_FOXNET_PACKET(ID, sz) PKTMAP[ID] = PacketInfo(FOXNET_PACKET_HANDLER(ID), sz);

namespace FoxNet {
    bool setSockNonblocking(SOCKET sock);

    class FoxPeer : public ByteStream {
    private:
        PktID currentPkt = PKTID_NONE;
        PktSize pktSize;
        bool alive = true;

    protected:
        PacketInfo PKTMAP[UINT8_MAX+1];
        SOCKET sock;

        int rawRecv(size_t sz);
        bool flushSend();

        PktSize getPacketSize(PktID);
        PktHandler getPacketHandler(PktID);

    public:
        FoxPeer();
        FoxPeer(SOCKET);

        /*
         * This should be called prior to writing packet data to the stream.
         */
        void prepareVarPacket(PktID id);

        /* 
         * After writing your variable-length packet to the stream, call this and it will patch the length.
         * make sure you call this BEFORE SENDING YOUR PACKET! remember, flushSend() is called after each packet
         * handler if there is data in the stream to send!
         */
        void patchVarPacket();

        // events
        virtual void onReady();

        bool isAlive();

        void kill();
        bool step();
    };
}