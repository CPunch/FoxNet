#pragma once

#include <string>
#include "FoxPeer.hpp"

namespace FoxNet {
    class FoxClient : public FoxPeer {
    private:
        PollFD fd;

    public:
        FoxClient(std::string ip, std::string port);
        ~FoxClient();

        // timeout in ms, if timeout is -1 poll() will block
        void pollPeer(int timeout);
    };
}