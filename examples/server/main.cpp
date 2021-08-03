#include "FoxServer.hpp"

#include <iostream>

int main() {
    FoxNet::FoxServer server(1337);
    
    while(1) {
        server.pollPeers(4000);
        std::cout << server.getPeerList().size() << " peers connected" << std::endl;
    }

    return 0;
}