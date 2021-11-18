#pragma once

#include "FoxNet.hpp"
#include "FoxPeer.hpp"

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

#ifdef __linux__
        struct epoll_event ev, ep_events[MAX_EPOLL_EVENTS];
        SOCKET epollfd;
#else
        std::vector<PollFD> fds; // raw poll descriptor
#endif

        void addFD(SOCKET fd) {
#ifdef __linux__
            ev.events = EPOLLIN;
            ev.data.fd = fd;
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                FOXFATAL("epoll_ctl [ADD] failed");
            }
#else
            fds.push_back({fd, POLLIN});
#endif
        }

        void deleteFD(SOCKET fd) {
#ifdef __linux__
            // epoll_event isn't needed with EPOLL_CTL_DEL, however we still need to pass a NON-NULL pointer. [see: https://man7.org/linux/man-pages/man2/epoll_ctl.2.html#BUGS]
            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev) == -1) {
                // non-fatal error, socket probably just didn't exist, so ignore it.
                FOXWARN("epoll_ctl [DEL] failed");
            }
#else
            for (auto iter = fds.begin(); iter != fds.end(); iter++) {
                if ((*iter).fd == fd) {
                    fds.erase(iter);
                    return;
                }
            }
#endif
        }

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
            _FoxNet_Init();

            // open our socket
            sock = socket(AF_INET, SOCK_STREAM, 0);
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

#ifdef __linux__
            // setup our epoll
            if ((epollfd = epoll_create(1)) == -1) {
                FOXFATAL("epoll_create() failed!");
            }
#endif

            addFD(sock);
        }

        ~FoxServer() {
            for (auto pair : peers) {
                peerType *peer = pair.second;
                peer->kill();
                delete peer;
            }

#ifdef __linux__
            close(epollfd);
#endif

            _FoxNet_Cleanup();
        }

        // used for connection keep-alive, actual packet will be sent on the next call to pollPeers()
        void pingPeers() {
            int64_t currTime = getTimestamp();

            // ping all peers (will be sent on next call to pollPeers())
            for (auto &pair : peers) {
                peerType *peer = pair.second; 
                peer->writeByte(PKTID_PING);
                peer->writeInt(currTime);
            }
        }

        // timeout in ms, if timeout is -1 poll() will block. returns true if an event was processed, or false if the timeout was triggered
        bool pollPeers(int timeout) {
            peerType *peer;
            SOCKET fd;
            bool pollIn;
            int events;

            // check if we have any queued outgoing packets
            for (auto pIter = peers.begin(); pIter != peers.end();) {
                peer = (*pIter).second;

                if (!peer->sendStep()) { // check if we have any outgoing packets, and if we failed to send, remove the peer
                    onPeerDisconnect(peer);

                    // remove peer from poll
                    deleteFD(peer->getRawSock());

                    // remove peer from peers map
                    peers.erase(pIter++);

                    // free peer
                    delete peer;
                } else {
                    ++pIter;
                }
            }

            // poll() blocks until there's an event to be handled on a file descriptor in the pollfd* list passed.
#ifdef __linux__
            events = epoll_wait(epollfd, ep_events, MAX_EPOLL_EVENTS, timeout);
#else
            events = poll(fds.data(), fds.size(), timeout); // poll returns -1 for error, or the number of events
#endif
            if (SOCKETERROR(events)) {
                FOXFATAL("poll() failed!");
            }

            // we hit our timeout, let caller know
            if (events == 0)
                return false;

#ifdef __linux__
            for (int i = 0; i < events; ++i) {
                struct epoll_event &event = ep_events[i];
                fd = event.data.fd;
                pollIn = event.events & EPOLLIN;
#else
            // walk through our fds vector, and only run the event on the sockets that require attention
            for (int i = 0; i < fds.size() && events > 0; ++i) {
                std::vector<PollFD>::iterator iter = fds.begin() + i;
                PollFD pfd = (*iter);

                // if nothing happened, skip!
                if (pfd.revents == 0)
                    continue;

                fd = pfd.fd;
                pollIn = pfd.revents & POLLIN;
                events--;
#endif

                if (fd == sock) { // event on our listener socket?
                    // if it wasn't a POLLIN, it was an error :/
                    if (!pollIn) {
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
                    addFD(newSock);
                    onNewPeer(peer);
                    continue;
                } // it's not the listener, must be a peer socket event

                // check if the peer still exists
                auto pIter = peers.find(fd);
                if (pIter == peers.end()) {
                    FOXWARN("event on unknown socket! closing and removing from queue...");

                    // closes the socket
                    killSocket(fd);

#ifndef __linux__
                    fds.erase(iter);
                    // decrements i so we'll be on the right iterator next iteration
                    i--;
#endif
                    continue;
                }

                peer = (*pIter).second;
                if (pollIn) { // is there data waiting to be read?
                    if (!peer->recvStep()) { // error occurred on socket
                        peer->kill();
                        goto _rmvPeer;
                    }
                } else { // peer disconnected or error occurred, just remove the peer
                _rmvPeer:
                    onPeerDisconnect(peer);
                    // remove peer
                    peers.erase(pIter);

                    // frees peer
                    delete peer;
#ifndef __linux__
                    fds.erase(iter);
                    // decrements i so we'll be on the right iterator next iteration
                    i--;
#endif
                    continue;
                }

#ifndef __linux__
                pfd.revents = 0;
#endif
            }

            return true;
        }

        std::map<SOCKET, peerType*>& getPeerList() {
            return peers;
        }
    };
}