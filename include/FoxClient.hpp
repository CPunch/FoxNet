#pragma once

#include <string>
#include "FoxPeer.hpp"
#include "FoxPoll.hpp"

namespace FoxNet {
    class FoxClient : public FoxPeer {
    private:
        FoxPollList pList;

    public:
        FoxClient(void);

        // NOTE: this function can throw a FoxException!
        void connect(std::string ip, std::string port);

        // timeout in ms, if timeout is -1 poll() will block
        // NOTE: this function can throw a FoxException!
        void pollPeer(int timeout);
    };
}