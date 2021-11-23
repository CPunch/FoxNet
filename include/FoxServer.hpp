#pragma once

#include "FoxNet.hpp"
#include "FoxPeer.hpp"
#include "FoxPoll.hpp"

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
        FoxPollList pollList;

        std::map<SOCKET, peerType*> peers; // matches socket to peer
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

            pollList.addSock(sock);
        }

        ~FoxServer() {
            for (auto pair : peers) {
                peerType *peer = pair.second;
                peer->kill();
                delete peer;
            }

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
            std::vector<FoxPollEvent> events;
            peerType *peer;

            // check if we have any queued outgoing packets
            for (auto pIter = peers.begin(); pIter != peers.end();) {
                peer = (*pIter).second;

                if (!peer->sendStep()) { // check if we have any outgoing packets, and if we failed to send, remove the peer
                    onPeerDisconnect(peer);

                    // remove peer from poll list
                    pollList.deleteSock(peer->getRawSock());

                    // remove peer from peers map
                    peers.erase(pIter++);

                    // free peer
                    delete peer;
                } else {
                    ++pIter;
                }
            }

            // poll() blocks until there's an event to be handled on a file descriptor in the pollfd* list passed.
            events = pollList.pollList(timeout);

            // if we have 0 events, it means we hit our timeout so let the caller know
            if (events.size() == 0)
                return false;

            for (auto iter = events.begin(); iter != events.end(); ++iter) {
                FoxPollEvent evnt = (*iter);

                if (evnt.sock == sock) { // event on our listener socket?
                    // if it wasn't a POLLIN, it was an error :/
                    if (!evnt.pollIn) {
                        FOXFATAL("error on listener socket!");
                    }

                    // grab the new connection
                    SOCKET newSock = accept(sock, (struct sockaddr *)&address, (socklen_t*)&addressSize);
                    if (SOCKETINVALID(newSock)) {
                        FOXWARN("failed to accept new connection!")
                        continue;
                    }

                    if (!setSockNonblocking(newSock)) {
                        FOXWARN("failed to set socket to non-blocking!")
                        killSocket(newSock);
                        continue;
                    }

                    peer = new peerType(newSock);

                    // add peer to map & set peer type & userdata
                    peers[newSock] = peer;

                    // add to poll list
                    pollList.addSock(newSock);
                    onNewPeer(peer);
                    continue;
                } // it's not the listener, must be a peer socket event

                // check if the peer still exists
                auto pIter = peers.find(evnt.sock);
                if (pIter == peers.end()) {
                    FOXWARN("event on unknown socket! closing and removing from queue...");

                    // remove it from the poll list & closes the socket
                    pollList.deleteSock(evnt.sock);
                    killSocket(evnt.sock);
                    continue;
                }

                peer = (*pIter).second;
                if (evnt.pollIn) { // is there data waiting to be read?
                    if (!peer->recvStep()) { // error occurred on socket
                        goto _rmvPeer;
                    }
                } else { // peer disconnected or error occurred, just remove the peer
                _rmvPeer:
                    // remove peer
                    peers.erase(pIter);

                    // removes sock from poll list & kills the peer
                    pollList.deleteSock(evnt.sock);
                    peer->kill();

                    // calls event
                    onPeerDisconnect(peer);

                    // frees peer
                    delete peer;
                }
            }

            return true;
        }

        std::map<SOCKET, peerType*>& getPeerList() {
            return peers;
        }
    };
}