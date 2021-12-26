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
    #define INVALID_SOCKET -1
    #define SOCKETINVALID(x) (x < 0)
    #define SOCKETERROR(x) (x == -1)
#endif
#include <fcntl.h>

#include "FoxNet.hpp"
#include "FoxPacket.hpp"
#include "ByteStream.hpp"

namespace FoxNet {
    void _FoxNet_Init(void);
    void _FoxNet_Cleanup(void);

    class FoxSocket : public ByteStream {
    private:
        SOCKET sock = INVALID_SOCKET;

    protected:

        enum RawSockCode {
            RAWSOCK_OK,
            RAWSOCK_ERROR,
            RAWSOCK_CLOSED,
            RAWSOCK_POLL
        };

        struct RawSockReturn {
            RawSockCode code;
            int processed;
        };

        RawSockReturn rawRecv(size_t sz); // reads bytes from socket
        RawSockReturn rawSend(size_t sz); // writes bytes to socket

    public:
        FoxSocket(void);
        ~FoxSocket(void);

        bool readBytes(Byte *out, size_t sz);
        void writeBytes(Byte *in, size_t sz);

        void connect(std::string ip, std::string port);
        void bind(uint16_t port); // bind socket to port
        void acceptFrom(FoxSocket *sock); // setup socket by accepting from another socket (note: host must have been bind()ed)
        bool setNonBlocking(void);

        virtual void onKilled(void); // fired when we have been killed (peer disconnect)
        virtual void onSend(Byte *data, size_t sz); // fired before data is sent over the socket
        virtual void onRecv(Byte *data, size_t sz); // fired after data was received from the socket, and is being read

        void kill(void);
        bool isAlive(void);
        SOCKET getRawSock(void);
    };
}