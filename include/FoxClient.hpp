#pragma once

#include <string>
#include "FoxPeer.hpp"

namespace FoxNet {
    class FoxClient : public FoxPeer {
    private:
        PollFD fd;

        DEF_FOXNET_PACKET(S2C_HANDSHAKE)

    public:
        FoxClient();
        ~FoxClient();

        // NOTE: this function can throw a FoxException!
        void connect(std::string ip, std::string port);

        // timeout in ms, if timeout is -1 poll() will block
        // NOTE: this function can throw a FoxException!
        void pollPeer(int timeout);
    };
}