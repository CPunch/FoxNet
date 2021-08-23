#include "FoxClient.hpp"
#include "../shared/UserPackets.hpp"

#include <iostream>

using namespace FoxNet;

class ExampleClient : public FoxClient {
private:
    DEF_FOXNET_PACKET(S2C_NUM_RESPONSE)

public:
    ExampleClient(std::string ip, std::string port): FoxClient(ip, port) {
        INIT_FOXNET_PACKET(S2C_NUM_RESPONSE, sizeof(uint32_t))
    }

    void onReady() {
        std::cout << "handshake accepted!" << std::endl;
        // write our addition request
        writeByte(C2S_REQ_ADD);
        writeUInt<uint32_t>(12);
        writeUInt<uint32_t>(56);
    }
};

DECLARE_FOXNET_PACKET(S2C_NUM_RESPONSE, ExampleClient) {
    uint32_t resp;

    peer->readUInt(resp);
    peer->flush();

    std::cout << "got result of " << resp << std::endl;
}

int main() {
    ExampleClient client("127.0.0.1", "1337");

    while (client.isAlive()) {
        client.pollPeer(-1);
    }

    return 0;
}