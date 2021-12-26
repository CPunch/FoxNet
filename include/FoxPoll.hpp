#pragma once

#include <iterator>
#include <cstddef>

#include "FoxSocket.hpp"

namespace FoxNet {
    struct FoxPollEvent {
        FoxSocket *sock;
        bool pollIn;
        bool pollOut;

        FoxPollEvent(FoxSocket*, bool, bool);
    };

    class FoxPollList {
    private:
#ifdef __linux__
        struct epoll_event ev, ep_events[MAX_EPOLL_EVENTS];
        SOCKET epollfd;
#else
        std::vector<PollFD> fds; // raw poll descriptor
#endif
        std::map<SOCKET, FoxSocket*> sockMap;

        void _setup(size_t res);

    public:
        FoxPollList(void);
        FoxPollList(size_t reserved);
        ~FoxPollList(void);

        void addSock(FoxSocket*);
        void rmvSock(FoxSocket*);
        void addPollOut(FoxSocket*);
        void rmvPollOut(FoxSocket*);

        std::vector<FoxPollEvent> pollList(int timeout);
        std::vector<FoxSocket*> getList(void);
    };
}