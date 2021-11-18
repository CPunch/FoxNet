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
        if (content)
            reqSendContent(content, 0);
        else { // file stream failed to open, kill the peer connection
            std::cout << "failed to open file stream! killing connection!" << std::endl;
            kill();
        }
    }

    void onContentSent(const ContentInfo info) {
        std::cout << "finished sending content! client should have it now." << std::endl;
        std::fclose(content);
    }
};

int main() {
    try {
        FoxNet::FoxServer<ExamplePeer> server(23337);

        while(1) {
            if (!server.pollPeers(1000)) { // if poll() didn't have any events to run, ping the peers
                server.pingPeers();
                std::cout << server.getPeerList().size() << " peers connected!" << std::endl;
            }
        }
    } catch(FoxNet::FoxException &e) {
        std::cerr << "Fatal Error! : " << e.what() << std::endl;
    }

    return 0;
}