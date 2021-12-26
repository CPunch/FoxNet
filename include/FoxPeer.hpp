#pragma once

#include <cstdio>
#include <map>
#include <unordered_set>

#include "ByteStream.hpp"
#include "FoxSocket.hpp"
#include "FoxPacket.hpp"
#include "FoxException.hpp"
#include "FoxPoll.hpp"

#define FOXNET_PACKET_HANDLER(ID) HANDLER_##ID

#define DEF_FOXNET_PACKET(ID) static void FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer);
#define DECLARE_FOXNET_PACKET(ID, className) void className::FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer)
#define INIT_FOXNET_PACKET(ID, sz) PKTMAP[ID].handler = FOXNET_PACKET_HANDLER(ID); PKTMAP[ID].size = sz; PKTMAP[ID].variable = false;

#define DEF_FOXNET_VAR_PACKET(ID) static void FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer, PktSize varSize);
#define DECLARE_FOXNET_VAR_PACKET(ID, className) void className::FOXNET_PACKET_HANDLER(ID)(FoxPeer *peer, PktSize varSize)
#define INIT_FOXNET_VAR_PACKET(ID) PKTMAP[ID].varhandler = FOXNET_PACKET_HANDLER(ID); PKTMAP[ID].size = 0; PKTMAP[ID].variable = true;

namespace FoxNet {
    class FoxPeer : public FoxSocket {
    private:
        PktID currentPkt = PKTID_NONE;
        PktSize pktSize;
        bool setPollOut = false;

        DEF_FOXNET_PACKET(PKTID_PING)
        DEF_FOXNET_PACKET(PKTID_PONG)

    protected:
        PacketInfo PKTMAP[UINT8_MAX+1];
        SOCKET sock;

        bool isPacketVar(PktID);
        PktSize getPacketSize(PktID);
        PktHandler getPacketHandler(PktID);
        PktVarHandler getVarPacketHandler(PktID);

    public:
        FoxPeer(void);

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
        virtual void onReady(void); // fired when we got a handshake response from the server and it went well :)
        virtual void onStep(void); // fired when sendStep() is called
        virtual void onPing(int64_t peerTime, int64_t currTime); // fired when PKTID_PING is received
        virtual void onPong(int64_t peerTime, int64_t currTime); // fired when PKTID_PONG is received

        bool handlePollIn(FoxPollList &plist);
        bool handlePollOut(FoxPollList &plist);

        SOCKET getRawSock(void);
    };
}