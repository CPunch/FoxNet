#include "FoxClient.hpp"
#include "../ArithShared/UserPackets.hpp"

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
        writeInt<uint32_t>(12);
        writeInt<uint32_t>(56);
    }

    void onPing(int64_t peerTime, int64_t currTime) {
        std::cout << "ping : " << currTime - peerTime << std::endl;
    }
};

DECLARE_FOXNET_PACKET(S2C_NUM_RESPONSE, ExampleClient) {
    uint32_t resp;

    peer->readInt(resp);

    std::cout << "got result of " << resp << std::endl;
}

int main() {
    try {
        ExampleClient client("127.0.0.1", "1337");

        while (client.isAlive()) {
            client.pollPeer(-1);
        }
    } catch(FoxNet::FoxException &e) {
        std::cerr << "Fatal Error! : " << e.what() << std::endl;
    }

    return 0;
}