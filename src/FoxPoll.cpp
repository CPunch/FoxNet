#include "FoxPoll.hpp"

using namespace FoxNet;

FoxPollEvent::FoxPollEvent(FoxSocket *s, bool pI, bool pO): sock(s), pollIn(pI), pollOut(pO) {}

void FoxPollList::_setup(size_t reserved) {
    _FoxNet_Init();

#ifdef __linux__
    memset(&ev, 0, sizeof(ev));
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

void FoxPollList::addSock(FoxSocket *sock) {
    SOCKET rawSock = sock->getRawSock();

    // add socket to map
    sockMap[rawSock] = sock;

#ifdef __linux__
    ev.events = EPOLLIN;
    ev.data.ptr = (void*)sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock->getRawSock(), &ev) == -1) {
        FOXFATAL("epoll_ctl [ADD] failed");
    }
#else
    fds.push_back({rawSock, POLLIN});
#endif
}

void FoxPollList::rmvSock(FoxSocket *sock) {
    SOCKET rawSock = sock->getRawSock();

    // remove from socket map
    sockMap.erase(rawSock);

#ifdef __linux__
    // epoll_event* isn't needed with EPOLL_CTL_DEL, however we still need to pass a NON-NULL pointer. [see: https://man7.org/linux/man-pages/man2/epoll_ctl.2.html#BUGS]
    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, rawSock, &ev) == -1) {
        // non-fatal error, socket probably just didn't exist, so ignore it.
        FOXWARN("epoll_ctl [DEL] failed");
    }
#else
    for (auto iter = fds.begin(); iter != fds.end(); iter++) {
        if ((*iter).fd == rawSock) {
            fds.erase(iter);
            return;
        }
    }
#endif
}

void FoxPollList::addPollOut(FoxSocket *sock) {
    SOCKET rawSock = sock->getRawSock();

#ifdef __linux__
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.ptr = (void*)sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sock->getRawSock(), &ev) == -1) {
        // non-fatal error, socket probably just didn't exist, so ignore it.
        FOXWARN("epoll_ctl [MOD] failed");
    }
#else
    for (auto iter = fds.begin(); iter != fds.end(); iter++) {
        if ((*iter).fd == rawSock) {
            (*iter).events = POLLIN | POLLOUT;
            return;
        }
    }
#endif
}

void FoxPollList::rmvPollOut(FoxSocket *sock) {
    SOCKET rawSock = sock->getRawSock();

#ifdef __linux__
    ev.events = EPOLLIN;
    ev.data.ptr = (void*)sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sock->getRawSock(), &ev) == -1) {
        // non-fatal error, socket probably just didn't exist, so ignore it.
        FOXWARN("epoll_ctl [MOD] failed");
    }
#else
    for (auto iter = fds.begin(); iter != fds.end(); iter++) {
        if ((*iter).fd == rawSock) {
            (*iter).events = POLLIN;
            return;
        }
    }
#endif
}

std::vector<FoxPollEvent> FoxPollList::pollList(int timeout) {
    std::vector<FoxPollEvent> events;
    int nEvents;

#ifdef __linux__
// fastpath: we store the FoxSocket* pointer directly in the epoll_data_t, saving us a lookup into our sockMap[].
//      not to mention the various improvements epoll() has over poll() :D
    nEvents = epoll_wait(epollfd, ep_events, MAX_EPOLL_EVENTS, timeout);

    if (SOCKETERROR(nEvents)) {
        FOXFATAL("epoll_wait() failed!");
    }

    for (int i = 0; i < nEvents; i++) {
        events.push_back(FoxPollEvent((FoxSocket*)ep_events[i].data.ptr, ep_events[i].events & EPOLLIN, ep_events[i].events & EPOLLOUT));
    }
#else
    nEvents = ::poll(fds.data(), fds.size(), timeout); // poll returns -1 for error, or the number of events

    if (SOCKETERROR(nEvents)) {
        FOXFATAL("poll() failed!");
    }

    // walk through the returned poll fds, if they have an event, add it to our events vector
    for (auto iter = fds.begin(); iter != fds.end() && nEvents > 0; iter++) {
        PollFD pfd = (*iter);
        if (pfd.revents != 0) {
            events.push_back(FoxPollEvent(sockMap[(SOCKET)pfd.fd], pfd.revents & POLLIN, pfd.revents & POLLOUT));
            --nEvents; // decrement the remaining events
        }
    }
#endif

    return events;
}

std::vector<FoxSocket*> FoxPollList::getList(void) {
    std::vector<FoxSocket*> sockList;
    sockList.reserve(sockMap.size());

    for (auto pair : sockMap) {
        sockList.push_back(pair.second);
    }

    return sockList;
}