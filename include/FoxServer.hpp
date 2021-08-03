#pragma once

#include "FoxNet.hpp"
#include "FoxPeer.hpp"

#define START_FDS 16

namespace FoxNet {
    class FoxServer : public FoxPeer {
    private:
        uint16_t port;
        socklen_t addressSize;
        struct sockaddr_in address;

        std::map<SOCKET, FoxPeer*> peers; // matches socket to peer
        std::vector<PollFD> fds; // raw poll descriptor
    public:
        FoxServer(uint16_t port);
        ~FoxServer();

        // timeout in ms, if timeout is -1 poll() will block
        void pollPeers(int timeout);

        std::map<SOCKET, FoxPeer*>& getPeerList();
    };
}