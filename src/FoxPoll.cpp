#include "FoxPoll.hpp"

using namespace FoxNet;

FoxPollEvent::FoxPollEvent(SOCKET s, bool p) {
    sock = s;
    pollIn = p;
}

void FoxPollList::_setup(size_t reserved) {
    _FoxNet_Init();

#ifdef __linux__
    // setup our epoll
    if ((epollfd = epoll_create(reserved)) == -1) {
        FOXFATAL("epoll_create() failed!");
    }
#else
    fds.reserve(reserved);
#endif
}

FoxPollList::FoxPollList(size_t res) {
    _setup(res);
}

FoxPollList::FoxPollList() {
    _setup(16); // default reserve size
}

FoxPollList::~FoxPollList() {
#ifdef __linux__
    close(epollfd);
#endif

    _FoxNet_Cleanup();
}

void FoxPollList::addSock(SOCKET sock) {
#ifdef __linux__
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1) {
        FOXFATAL("epoll_ctl [ADD] failed");
    }
#else
    fds.push_back({sock, POLLIN});
#endif
}

void FoxPollList::deleteSock(SOCKET sock) {
#ifdef __linux__
    // epoll_event* isn't needed with EPOLL_CTL_DEL, however we still need to pass a NON-NULL pointer. [see: https://man7.org/linux/man-pages/man2/epoll_ctl.2.html#BUGS]
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, &ev) == -1) {
        // non-fatal error, socket probably just didn't exist, so ignore it.
        FOXWARN("epoll_ctl [DEL] failed");
    }
#else
    for (auto iter = fds.begin(); iter != fds.end(); iter++) {
        if ((*iter).fd == sock) {
            fds.erase(iter);
            return;
        }
    }
#endif
}

std::vector<FoxPollEvent> FoxPollList::pollList(int timeout) {
    std::vector<FoxPollEvent> events;
    int nEvents;

#ifdef __linux__
    nEvents = epoll_wait(epollfd, ep_events, MAX_EPOLL_EVENTS, timeout);

    if (SOCKETERROR(nEvents)) {
        FOXFATAL("epoll_wait() failed!");
    }

    for (int i = 0; i < nEvents; i++) {
        events.push_back(FoxPollEvent(ep_events[i].data.fd, ep_events[i].events & EPOLLIN));
    }
#else
    nEvents = poll(fds.data(), fds.size(), timeout); // poll returns -1 for error, or the number of events

    if (SOCKETERROR(nEvents)) {
        FOXFATAL("poll() failed!");
    }

    // walk through the returned poll fds, if they have an event, add it to our events vector
    for (auto iter = fds.begin(); iter != fds.end() && nEvents > 0; iter++) {
        PollFD pfd = (*iter);
        if (pfd.revents != 0) {
            events.push_back(FoxPollEvent(pfd.fd, pfd.revents & POLLIN));
            --nEvents; // decrement the remaining events
        }
    }
#endif

    return events;
}