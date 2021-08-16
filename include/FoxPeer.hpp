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

namespace FoxNet {
    bool setSockNonblocking(SOCKET sock);

    typedef enum {
        PEER_CLIENT,
        PEER_SERVER
    } PEERTYPE;

    typedef void (*EventCallback)(FoxPeer *peer);

    typedef enum {
        PEEREVENT_ONREADY, /* for PEER_CLIENT this is fired whenever the handshake response is accepted */
        PEEREVENT_MAX
    } PEEREVENT;

    class FoxPeer : public ByteStream {
    protected:
        EventCallback events[PEEREVENT_MAX];
        bool alive = true;
        PktID currentPkt = PKTID_NONE;
        void *userdata = nullptr;
        SOCKET sock;
        uint16_t pktSize;

        int rawRecv(size_t sz);
        bool flushSend();

    public:
        PEERTYPE type = PEER_CLIENT;
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

        bool callEvent(PEEREVENT id);
        bool isAlive();

        void setHndlerUData(void*);
        void setEvent(PEEREVENT eventID, EventCallback callback);

        void kill();
        bool step();
    };
}