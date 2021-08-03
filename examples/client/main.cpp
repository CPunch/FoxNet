#include "FoxClient.hpp"

int main() {
    FoxNet::FoxClient client("127.0.0.1", "1337");

    while (client.isAlive()) {
        client.pollPeer(-1);
    }

    return 0;
}