#include "FoxServer.hpp"
#include "../ArithShared/UserPackets.hpp"

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

    peer->readInt<uint32_t>(a);
    peer->readInt<uint32_t>(b);

    std::cout << "got (" << a << ", " << b << ")" << std::endl;

    // perform advanced intensive arithmatic operation for our client
    res = a + b;

    peer->writeByte(S2C_NUM_RESPONSE);
    peer->writeInt<uint32_t>(res);
}

int main() {
    FoxNet::Init();
    FoxNet::FoxServer<ExamplePeer> server(1337);

    while(1) {
        if (!server.pollPeers(3000)) // if out timeout was triggered, poll peers
            server.pingPeers();
        std::cout << server.getPeerList().size() << " peers connected" << std::endl;
    }

    FoxNet::Cleanup();
    return 0;
}