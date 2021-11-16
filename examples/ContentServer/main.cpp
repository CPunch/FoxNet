#include "FoxServer.hpp"

using namespace FoxNet;

class ExamplePeer : public FoxServerPeer {
private:
    std::FILE *content;

public:
    ExamplePeer() {
        content = std::fopen("content.txt", "rb");
    }

    ExamplePeer(SOCKET sock) : FoxServerPeer(sock) {
        content = std::fopen("content.txt", "rb");
    }

    void onReady() {
        reqSendContent(content, 0);
    }

    void onContentSent(const ContentInfo info) {
        std::cout << "finished sending content! client should have it now." << std::endl;
    }
};

int main() {
    FoxNet::Init();
    FoxNet::FoxServer<ExamplePeer> server(23337);

    while(1) {
        server.pollPeers(1000);
        server.pingPeers();
    }

    FoxNet::Cleanup();
    return 0;
}