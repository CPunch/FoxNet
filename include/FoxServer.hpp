#pragma once

#include "FoxNet.hpp"
#include "FoxPeer.hpp"

#define START_FDS 16

namespace FoxNet {
    // base FoxServer peer class, make a parent class of this and add your own custom packet ids
    class FoxServerPeer : public FoxPeer {
    private:
        void setupPackets();

    public:
        FoxServerPeer();
        FoxServerPeer(SOCKET sock);

        DEF_FOXNET_PACKET(C2S_HANDSHAKE)
    };

    template<typename peerType>
    class FoxServer {
        static_assert(std::is_base_of<FoxServerPeer, peerType>::value, "peerType must derive from FoxServerPeer");

    private:
        SOCKET sock;
        uint16_t port;
        socklen_t addressSize;
        struct sockaddr_in address;

        std::map<SOCKET, peerType*> peers; // matches socket to peer
        std::vector<PollFD> fds; // raw poll descriptor
    public:

        // events !
        virtual void onNewPeer(peerType *peer) {
            std::cout << "New peer " << peer << " connected!" << std::endl;
        }

        virtual void onPeerDisconnect(peerType *peer) {
            std::cout << "Peer " << peer << " disconnected!" << std::endl;
        }

// ============================================= [[ Base FoxServer implementation ]] =============================================

        FoxServer(uint16_t p): port(p) {
            // open our socket
            sock =  socket(AF_INET, SOCK_STREAM, 0);
            if (SOCKETINVALID(sock)) {
                FOXFATAL("socket() failed!");
            }

            // attach socket to the port
            int opt = 1;
#ifdef _WIN32
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) {
#else
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
#endif
                FOXFATAL("setsockopt() failed!");
            }
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            addressSize = sizeof(address);

            // bind to the port
            if (SOCKETERROR(bind(sock, (struct sockaddr *)&address, addressSize))) {
                FOXFATAL("bind() failed!");
            }

            if (SOCKETERROR(listen(sock, SOMAXCONN))) {
                FOXFATAL("listen() failed!");
            }

            // set server listener to non-blocking
            if (!setSockNonblocking(sock)) {
                FOXFATAL("failed to unblock listener socket!");
            }

            fds.reserve(START_FDS);
            fds.push_back({sock, POLLIN});
        }

        ~FoxServer() {
            for (auto pair : peers) {
                FoxPeer *peer = pair.second;
                peer->kill();
                delete peer;
            }
        }

        // timeout in ms, if timeout is -1 poll() will block
        void pollPeers(int timeout) {
            // poll() blocks until there's an event to be handled on a file descriptor in the pollfd* list passed. 
            int events = poll(fds.data(), fds.size(), timeout); // poll returns -1 for error, or the number of events
            if (SOCKETERROR(events)) {
                FOXFATAL("poll() failed!");
            }

            // walk through our fds vector, and only run the event on the sockets that require attention
            for (int i = 0; i < fds.size() && events > 0; ++i) {
                std::vector<PollFD>::iterator iter = fds.begin() + i;
                PollFD fd = (*iter);

                // if nothing happened, skip!
                if (fd.revents == 0)
                    continue;

                events--;
                if (fd.fd == sock) { // event on our listener socket?
                    // if it wasn't a POLLIN, it was an error :/
                    if (fd.revents & ~POLLIN) {
                        FOXFATAL("error on listener socket!");
                    }

                    // grab the new connection
                    SOCKET newSock = accept(sock, (struct sockaddr *)&address, (socklen_t*)&addressSize);
                    if (SOCKETINVALID(newSock)) {
                        continue;
                    }

                    if (!setSockNonblocking(newSock))
                        continue;

                    peerType *peer = new peerType(newSock);

                    // add peer to map & set peer type & userdata
                    peers[newSock] = peer;

                    // add to pollfd vector
                    fds.push_back({newSock, POLLIN});
                    onNewPeer(peer);
                    continue;
                } // it's not the listener, must be a peer socket event

                // check if the peer still exists
                auto pIter = peers.find(fd.fd);
                if (pIter == peers.end()) {
                    FOXWARN("event on unknown socket! closing and removing from queue...");

                    // erases from the fds vector, decrements i so we'll be on the right iterator next iteration and closes the socket
                    fds.erase(iter);
                    i--;

        #ifdef _WIN32
                    shutdown(fd.fd, SD_BOTH);
                    closesocket(fd.fd);
        #else
                    shutdown(fd.fd, SHUT_RDWR);
                    close(fd.fd);
        #endif
                    continue;
                }

                peerType *peer = (*pIter).second;

                // it's an error if it's not a POLLIN event
                if (fd.revents & ~POLLIN) {
        _rmvPeer:
                    onPeerDisconnect(peer);
                    // free peer & remove it from the map
                    delete peer;
                    peers.erase(pIter);

                    // erases from the fds vector, decrements i so we'll be on the right iterator next iteration
                    fds.erase(iter);
                    i--;
                    continue;
                } else if (!peer->step()) { // error occured on socket
                    peer->kill();
                    goto _rmvPeer;
                }
            }
        }

        std::map<SOCKET, peerType*>& getPeerList() {
            return peers;
        }
    };
}