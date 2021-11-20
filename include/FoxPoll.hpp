#pragma once

#include <iterator>
#include <cstddef>

#include "FoxPeer.hpp"

namespace FoxNet {
    struct FoxPollEvent {
        SOCKET sock;
        bool pollIn;

        FoxPollEvent(SOCKET, bool);
    };

    class FoxPollList {
    private:
#ifdef __linux__
        struct epoll_event ev, ep_events[MAX_EPOLL_EVENTS];
        SOCKET epollfd;
#else
        std::vector<PollFD> fds; // raw poll descriptor
#endif

        void _setup(size_t res);

    public:
        FoxPollList();
        FoxPollList(size_t reserved);
        ~FoxPollList();

        void addSock(SOCKET fd);
        void deleteSock(SOCKET fd);

        std::vector<FoxPollEvent> pollList(int timeout);
    };
}