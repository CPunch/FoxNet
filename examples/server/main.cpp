#include "FoxServer.hpp"
#include "../shared/UserPackets.hpp"

#include <iostream>

void HANDLE_C2S_ADD(FoxNet::FoxPeer *peer, void *udata) {
    uint32_t a, b, res;

    peer->readUInt(a);
    peer->readUInt(b);
    peer->flush();

    std::cout << "got (" << a << ", " << b << ")" << std::endl;

    // perform advanced intensive arithmatic operation for our client
    res = a + b;

    peer->writeByte(S2C_NUM_RESPONSE);
    peer->writeUInt(res);
}

int main() {
    FoxNet::FoxServer server(1337);

    FoxNet::registerUserPacket(C2S_REQ_ADD, HANDLE_C2S_ADD, PEER_SERVER, sizeof(uint32_t) + sizeof(uint32_t));
    while(1) {
        server.pollPeers(4000);
        std::cout << server.getPeerList().size() << " peers connected" << std::endl;
    }

    return 0;
}