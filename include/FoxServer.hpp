#pragma once

#include "FoxNet.hpp"
#include "FoxSocket.hpp"
#include "FoxPeer.hpp"
#include "FoxPoll.hpp"

namespace FoxNet {
    // base FoxServer peer class, make a parent class of this and add your own custom packet ids
    class FoxServerPeer : public FoxPeer {
    public:
        FoxServerPeer(void);
    };

    template<typename peerType>
    class FoxServer : FoxSocket {
        static_assert(std::is_base_of<FoxServerPeer, peerType>::value, "peerType must derive from FoxServerPeer");

    private:
        SOCKET sock;
        uint16_t port;
        socklen_t addressSize;
        struct sockaddr_in address;
        FoxPollList pollList;

        void killPeer(peerType *peer) {
            onPeerDisconnect(peer);
            pollList.rmvSock(peer);
            delete peer;
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
            // binds the socket a port
            bind(p);

            pollList.addSock(this);
        }

        ~FoxServer() {
            std::vector<FoxSocket*> peers = pollList.getList();

            for (FoxSocket* peer : peers) {
                if (peer == this) // skip us
                    continue;

                delete dynamic_cast<peerType*>(peer);
            }
        }

        // used for connection keep-alive, actual packet will be sent on the next call to pollPeers()
        void pingPeers() {
            int64_t currTime = getTimestamp();

            // ping all peers (will be sent on next call to pollPeers())
            for (FoxSocket *peer : pollList.getList()) {
                peer->writeByte(PKTID_PING);
                peer->writeInt(currTime);
            }
        }

        // timeout in ms, if timeout is -1 poll() will block. returns true if an event was processed, or false if the timeout was triggered
        bool pollPeers(int timeout) {
            std::vector<FoxPollEvent> events;
            peerType *peer;

            events = pollList.pollList(timeout);

            if (events.size() == 0) // no events to handle, out timeout must've ran out
                return false;

            for (FoxPollEvent &e : events) {
                // check if event was on our bound port
                if (e.sock == this) {
                    peer = new peerType();

                    // accept the new connection :D
                    peer->acceptFrom(this);
                    peer->setNonBlocking();

                    onNewPeer(peer);
                    pollList.addSock(dynamic_cast<FoxSocket*>(peer));
                    continue;
                }

                // grab peer
                peer = dynamic_cast<peerType*>(e.sock);

                // handle poll events
                try {
                    if (e.pollIn && !peer->handlePollIn(pollList))
                        killPeer(peer);

                    if (e.pollOut && !peer->handlePollOut(pollList))
                        killPeer(peer);

                    if (!e.pollIn && !e.pollOut) // no normal event = connection reset or error
                        killPeer(peer);
                } catch(...) {
                    killPeer(peer);
                }
            }

            return true;
        }

        std::vector<peerType*> getPeerList() {
            std::vector<peerType*> groomedPeers;
            std::vector<FoxSocket*> peers = pollList.getList();

            groomedPeers.reserve(peers.size() - 1);
            for (FoxSocket *peer : peers) {
                if (peer == this)
                    continue;

                groomedPeers.push_back(dynamic_cast<peerType*>(peer));
            }

            return groomedPeers;
        }
    };
}