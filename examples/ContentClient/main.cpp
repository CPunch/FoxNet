#include "FoxClient.hpp"

#include <iomanip>

using namespace FoxNet;

class ExampleClient : public FoxClient {
public:
    ExampleClient(std::string ip, std::string p) : FoxClient(ip, p) {}

    void onPing(int64_t peerTime, int64_t currTime) {
        std::cout << "ping : " << currTime - peerTime << std::endl;
    }

    bool onContentRequest(uint8_t type, const ContentInfo content) {
        return true; // accept all content requests
    }

    void onContentReceived(const ContentInfo content) {
        char *buf = new char[content.size];

        std::cout << "received full content buffer! hash : { " << std::endl;

        for (size_t i = 0; i < 32; i++)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)content.hash[i] << std::dec << " ";

        std::cout << "} buff : ";

        // read content
        fread(buf, 1, content.size, content.file);

        for (int i = 0; i < content.size; i++)
            std::cout << buf[i];
        
        std::cout << std::endl;

        delete[] buf;
    }
};

int main() {
    ExampleClient client("127.0.0.1", "23337");

    while (client.isAlive()) {
        client.pollPeer(1000);
    }

    return 0;
}