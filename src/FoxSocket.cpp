#include "FoxSocket.hpp"

#include <mutex>

using namespace FoxNet;

// if _FNSetup > 0, WSA has already been started. if _FNSetup == 0, WSA needs to be cleaned up
static int _FNSetup = 0;
static std::mutex _FNSetupLock;

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

FoxSocket::RawSockReturn FoxSocket::rawRecv(size_t sz) {
    RawSockCode errCode = RAWSOCK_OK;
    int rcvd;
    int start = inBuffer.size();

    inBuffer.resize(start + sz);
    rcvd = ::recv(sock, (buffer_t*)(inBuffer.data() + start), sz, 0);

    if (rcvd == 0) {
        errCode = RAWSOCK_CLOSED;
    } else if (SOCKETERROR(rcvd) && FN_ERRNO != FN_EWOULD
#ifndef _WIN32
        // if it's a posix system, also make sure its not a EAGAIN result (which is a recoverable error, there's just nothing to read lol)
        && FN_ERRNO != EAGAIN
#endif
    ) {
        // if the socket closed or an error occurred, return the error result
        errCode = RAWSOCK_ERROR;
    } else if (rcvd > 0) {
        // trim excess
        inBuffer.resize(start + rcvd);
    }

    return {errCode, rcvd};
}

FoxSocket::RawSockReturn FoxSocket::rawSend(size_t sz) {
    RawSockCode errCode = RAWSOCK_OK;
    int sentBytes = 0;
    int sent;

    // write bytes to the socket until an error occurs or we finish sending
    do {
        sent = ::send(sock, (buffer_t*)(outBuffer.data() + sentBytes), sz - sentBytes, 0);

        // check for error result
        if (sent == 0) { // connection closed gracefully
            errCode = RAWSOCK_CLOSED;
            goto _rawWriteExit;
        } else if (SOCKETERROR(sent)) { // socket error?
            if (FN_ERRNO != FN_EWOULD
#ifndef _WIN32
                // posix also has some platforms which define EAGAIN as a different value than EWOULD, might as well support it.
                && FN_ERRNO != EAGAIN
#endif
            ) { // socket error!
                errCode = RAWSOCK_ERROR;
                goto _rawWriteExit;
            }

            // it was a result of EWOULD or EAGAIN, kernel socket send buffer is full,
            // tell the caller we need to set our poll event POLLOUT
            errCode = RAWSOCK_POLL;
            goto _rawWriteExit;
        }
    } while((sentBytes += sent) < sz);

_rawWriteExit:
    // trim
    if (sentBytes > 0)
        outBuffer.erase(outBuffer.begin(), outBuffer.begin() + sentBytes);
    return {errCode, sentBytes};
}

FoxSocket::FoxSocket(void) {
    _FoxNet_Init();
}

// the base class is deconstructed last, so it's the perfect place to cleanup WSA (if we're on windows)
FoxSocket::~FoxSocket(void) {
    kill();
    _FoxNet_Cleanup();
}

bool FoxSocket::readBytes(Byte *out, size_t sz) {
    if (ByteStream::readBytes(out, sz)) {
        onRecv(out, sz);
        return true;
    }

    return false;
}

void FoxSocket::writeBytes(Byte *in, size_t sz) {
    ByteStream::writeBytes(in, sz);
    onSend((outBuffer.data() + outBuffer.size() - sz), sz);
}

void FoxSocket::connect(std::string ip, std::string port) {
    struct addrinfo res, *result, *curr;

    if (!SOCKETINVALID(sock)) {
        FOXFATAL("socket already setup!")
    }

    // zero out our address info and setup the type
    memset(&res, 0, sizeof(addrinfo));
    res.ai_family = AF_UNSPEC;
    res.ai_socktype = SOCK_STREAM;

    // grab the address info
    if (::getaddrinfo(ip.c_str(), port.c_str(), &res, &result) != 0) {
        FOXFATAL("getaddrinfo() failed!");
    }

    // getaddrinfo returns a list of possible addresses, step through them and try them until we find a valid address
    for (curr = result; curr != NULL; curr = curr->ai_next) {
        sock = ::socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);

        // if it failed, try the next sock
        if (SOCKETINVALID(sock))
            continue;
        
        // if it's not an invalid socket, break and exit the loop, we found a working addr!
        if (!SOCKETINVALID(::connect(sock, curr->ai_addr, curr->ai_addrlen)))
            break;

        kill();
    }
    freeaddrinfo(result);

    // if we reached the end of the linked list, we failed looking up the addr
    if (curr == NULL) {
        FOXFATAL("couldn't connect a valid address handle to socket!");
    }
}

void FoxSocket::bind(uint16_t port) {
    socklen_t addressSize;
    struct sockaddr_in address;

    if (!SOCKETINVALID(sock)) {
        FOXFATAL("socket already setup!")
    }

    // open our socket
    sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKETINVALID(sock)) {
        FOXFATAL("socket() failed!");
    }

    // attach socket to the port
    int opt = 1;
#ifdef _WIN32
    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) {
#else
    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
#endif
        FOXFATAL("setsockopt() failed!");
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    addressSize = sizeof(address);

    // bind to the port
    if (SOCKETERROR(::bind(sock, (struct sockaddr *)&address, addressSize))) {
        FOXFATAL("bind() failed!");
    }

    if (SOCKETERROR(::listen(sock, SOMAXCONN))) {
        FOXFATAL("listen() failed!");
    }
}

void FoxSocket::acceptFrom(FoxSocket *host) {
    socklen_t addressSize;
    struct sockaddr address;

    sock = ::accept(host->getRawSock(), &address, &addressSize);
    if (SOCKETINVALID(sock)) {
        FOXFATAL("accept() failed!")
    }
}

bool FoxSocket::setNonBlocking(void) {
#ifdef _WIN32
    unsigned long mode = 1;
    if (::ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
    if (::fcntl(sock, F_SETFL, (::fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
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

void FoxSocket::onKilled(void) {
    // stubbed
}

void FoxSocket::onSend(Byte *data, size_t sz) {
    // stubbed
}

void FoxSocket::onRecv(Byte *data, size_t sz) {
    // stubbed
}

void FoxSocket::kill(void) {
    if (!isAlive())
        return;

    onKilled();

#ifdef _WIN32
    shutdown(sock, SD_BOTH);
    closesocket(sock);
#else
    shutdown(sock, SHUT_RDWR);
    close(sock);
#endif

    sock = INVALID_SOCKET;
}

bool FoxSocket::isAlive(void) {
    return sock != INVALID_SOCKET;
}

SOCKET FoxSocket::getRawSock(void) {
    return sock;
}