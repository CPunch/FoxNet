#include "FoxServer.hpp"
#include "../shared/UserPackets.hpp"

#include <iostream>

class ExamplePeer : public FoxServerPeer {
private:
    DEF_FOXNET_PACKET(C2S_REQ_ADD);

public:
    ExamplePeer(SOCKET sock): FoxServerPeer(sock) {
        INIT_FOXNET_PACKET(C2S_REQ_ADD, sizeof(uint32_t) + sizeof(uint32_t))
    }
};

DECLARE_FOXNET_PACKET(C2S_REQ_ADD, ExamplePeer) {
    uint32_t a, b, res;

    peer->readUInt(a);
    peer->readUInt(b);

    std::cout << "got (" << a << ", " << b << ")" << std::endl;

    // perform advanced intensive arithmatic operation for our client
    res = a + b;

    peer->writeByte(S2C_NUM_RESPONSE);
    peer->writeUInt(res);
}

int main() {
    FoxNet::FoxServer<ExamplePeer> server(1337);

    while(1) {
        server.pollPeers(4000);
        std::cout << server.getPeerList().size() << " peers connected" << std::endl;
    }

    return 0;
}