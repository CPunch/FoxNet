/*  Free Open Xross Network

    Author: CPunch <3

    This file is a single-header-library for a basic TCP cross-platform network stack (think ZMQ, but very basic). Built to handle hundreds of clients. 

    This was (re)written after working on OpenFusion where I learned a lot about how to do networking properly (aka, not how FusionFall did it LOL)
*/

#ifndef _FoxNet_PROTOCOL
#define _FoxNet_PROTOCOL

#include <iostream>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#ifndef _WIN32
// posix platform
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int SOCKET;
    #define SOCKETINVALID(x) (x < 0)
    #define SOCKETERROR(x) (x == -1)
#else
// windows (UNTESTED)
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    #define errno WSAGetLastError()
    #define SOCKETINVALID(x) (x == INVALID_SOCKET)
    #define SOCKETERROR(x) (x == SOCKET_ERROR)
#endif
#include <fcntl.h>

#include <string>
#include <thread>
#include <mutex>
#include <list>
#include <map>

#define MAX_PACKETSIZE 1024 * 8

namespace FoxNet {
    // INIT/QUIT (mainly for windows bc windows SUCKKKKKSSSSSSS)
    inline void init() {
#ifdef _WIN32
        WSADATA wsa_data;
        return WSAStartup(MAKEWORD(1,1), &wsa_data);
#endif
    }

    inline void cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // wrapper for calloc with error checking
    inline void* xmalloc(size_t sz) {
        void* buf = calloc(1, sz);
        if (buf == NULL) {
            std::cerr << "[FATAL] FoxNet: calloc failed!" << std::endl;
            exit(EXIT_FAILURE);
        }
        return buf;
    }

    struct FoxPacket {
        void* data; // pointer to the data
        uint32_t size; // size of data, this means the max amount of data we can send is 4Gb
        uint16_t type;

        FoxPacket(uint16_t t, void* d = NULL, uint32_t s = 0): 
            size(s), data(d), type(t) {}
    };

    class FoxConnection {
    private:
        SOCKET sock;
        void* recvBuffer;
        uint32_t recvSize = 0;
        int recvIndex = 0;
        bool activelyReading = false;
        bool alive = true;

        bool sendData(void* data, int size) {
            int sentBytes = 0;

            while (sentBytes < size) {
        #ifdef _WIN32
                int sent = send(sock, (char*)((uint8_t*)data + sentBytes), size - sentBytes, 0); // no flags defined
        #else 
                int sent = send(sock, (void*)((uint8_t*)data + sentBytes), size - sentBytes, 0); // no flags defined
        #endif
                if (SOCKETERROR(sent)) 
                    return false; // error occured while sending bytes
                sentBytes += sent;
            }

            return true; // it worked!
        }

    public:
        FoxConnection() {}
        FoxConnection(SOCKET s): 
            sock(s) {}

        bool isAlive() {
            return alive;
        }

        void kill() {
            alive = false;
#ifdef _WIN32
            shutdown(sock, SD_BOTH);
            closesocket(sock);
#else
            shutdown(sock, SHUT_RDWR);
            close(sock);
#endif
        }

        void sendPacket(FoxPacket data) {
            uint32_t pSize = htonl(data.size + sizeof(uint16_t));
            uint16_t pType = htons(data.type);

            // send size of the pack (includes type)
            if (!sendData((void*)&pSize, sizeof(uint32_t))) { // failed to send the data
                kill();
                return;
            }

            if (!sendData((void*)&pType, sizeof(uint16_t))) {
                kill();
                return;
            }

            if (!sendData(data.data, data.size)) {
                kill();
                return;
            }
        }

        FoxPacket* step() {
            if (!activelyReading) {
                // read packet size
#ifdef _WIN32
                int recved = recv(sock, (char*)&recvSize, sizeof(uint32_t), 0);
#else
                int recved = recv(sock, (void*)&recvSize, sizeof(uint32_t), 0);
#endif

                // sanity checks 0 = shutdown, -1 = error, either way, close this socket.
                if (SOCKETERROR(recved) && errno != EAGAIN) {
                    kill();
                    return NULL;
                }
                recvSize = ntohl(recvSize);

                if (recvSize == 0) {
                    return NULL;
                }

                if (recvSize > MAX_PACKETSIZE) {
                    kill();
                    return NULL;
                }

                activelyReading = true;
                recvBuffer = xmalloc(recvSize);
            }

            if (activelyReading && recvIndex < recvSize) {
#ifdef _WIN32
                int recved = recv(sock, (char*)((uint8_t*)recvBuffer + recvIndex), recvSize, 0);
#else
                int recved = recv(sock, (void*)((uint8_t*)recvBuffer + recvIndex), recvSize, 0);
#endif
                if (SOCKETERROR(recved) && errno != EAGAIN) {
                    kill();
                    return NULL;
                } else {
                    recvIndex += recved;
                }
            }

            // check if we've finished reading the packet!
            if (activelyReading && recvIndex - recvSize == 0) {
                // copy buffer without packet type
                int tmpSize = recvSize - sizeof(uint16_t);
                void* tmpBuf = xmalloc(tmpSize);
                memcpy(tmpBuf, (void*)((uint8_t*)recvBuffer + sizeof(uint16_t)), tmpSize);

                // grab packet type
                uint16_t pType = *((uint16_t*)recvBuffer);
                pType = ntohs(pType);

                FoxPacket* packet = new FoxPacket(pType, tmpBuf, tmpSize);

                // reset everything
                free(recvBuffer);
                recvBuffer = NULL;
                recvSize = 0;
                recvIndex = 0;
                activelyReading = false;

                // return allocated packet!
                return packet;
            }

            return NULL;
        }
    };

    typedef void(*FoxPacketHandler)(FoxConnection*, FoxPacket*);

    class FoxServer {
    private:
        std::list<FoxConnection*> connections;
        std::map<uint16_t, FoxPacketHandler> packetMap;

        std::mutex activeCritical;

        SOCKET sock;
        uint16_t port;
        socklen_t addressSize;
        struct sockaddr_in address;

        void packetHandler(FoxConnection* sock, FoxPacket* data) {
            if (packetMap.find(data->type) == packetMap.end()) {
                std::cerr << "[WARN] FoxNet: unknown packet type [" << data->type << "]" << std::endl;
                return;
            }

            packetMap[data->type](sock, data);
        }

    public:
        FoxServer(uint16_t p): port(p) {
            // create socket file descriptor 
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (SOCKETINVALID(sock)) { 
                std::cerr << "[FATAL] FoxNet: socket failed" << std::endl; 
                exit(EXIT_FAILURE); 
            }

            // attach socket to the port
            int opt = 1;
#ifdef _WIN32
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) { 
#else
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) { 
#endif
                std::cerr << "[FATAL] FoxNet: setsockopt failed" << std::endl; 
                exit(EXIT_FAILURE); 
            } 
            address.sin_family = AF_INET; 
            address.sin_addr.s_addr = INADDR_ANY; 
            address.sin_port = htons(port); 

            addressSize = sizeof(address);

            // Bind to the port
            if (SOCKETERROR(bind(sock, (struct sockaddr *)&address, addressSize))) { 
                std::cerr << "[FATAL] FoxNet: bind failed" << std::endl; 
                exit(EXIT_FAILURE); 
            }

            if (SOCKETERROR(listen(sock, SOMAXCONN))) {
                std::cerr << "[FATAL] FoxNet: listen failed" << std::endl; 
                exit(EXIT_FAILURE); 
            }

            // set server listener to non-blocking
#ifdef _WIN32
            unsigned long mode = 1;
            if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
            if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                std::cerr << "[FATAL] FoxNet: fcntl failed" << std::endl; 
                exit(EXIT_FAILURE); 
            }
        }

        void setPacket(uint16_t ID, FoxPacketHandler handler) {
            packetMap[ID] = handler;
        }

        // will automatically free FoxPacket when done
        void sendPacketToAll(FoxPacket data) {
            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends

            for (FoxConnection* sock: connections) {
                if (sock->isAlive()) {
                    sock->sendPacket(data);
                }
                // let step() deal with all the dead connections /shrug
            }
        }

        void step() {
            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends

            SOCKET newConnection = accept(sock, (struct sockaddr *)&(address), (socklen_t*)&(addressSize));
            if (!SOCKETINVALID(newConnection)) {
                // new connection! make sure to set non-blocking!
#ifdef _WIN32
                unsigned long mode = 1;
                if (ioctlsocket(newConnection, FIONBIO, &mode) != 0) {
#else
                if (fcntl(newConnection, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                    std::cerr << "[FATAL] FoxNet: fcntl failed on new connection" << std::endl; 
                    exit(EXIT_FAILURE); 
                }

                std::cout << "New connection! " << inet_ntoa(address.sin_addr) << std::endl;

                // add connection to list!
                FoxConnection* tmp = new FoxConnection(newConnection);
                connections.push_back(tmp);
            }

            // for each active connection, step()
            std::list<FoxConnection*>::iterator i = connections.begin();
            while (i != connections.end()) {
                FoxConnection* current = *i;

                if (!current->isAlive()) { // remove inactive connections
                    delete current;
                    connections.erase(i++);
                    continue;
                }

                FoxPacket* packet = current->step();
                if (packet != NULL) { // we got a packet!
                    packetHandler(current, packet);
                    free(packet->data);
                    delete packet;
                }

                // go to the next element in the list
                ++i;
            }
        }

        // meant to be started in a seperate thread
        void start() {
            while (true) {
                step();
                sleep(0); // lets the OS run other threads so we're not hitting 100% CPU
            }
        }
    };

    class FoxClient {
    private:
        std::map<uint16_t, FoxPacketHandler> packetMap;

        FoxConnection connection;
        std::string address;
        uint16_t port;

        void packetHandler(FoxConnection* sock, FoxPacket* data) {
            std::cout << "got packet: " << data->type << std::endl;
            if (packetMap.find(data->type) == packetMap.end()) {
                std::cerr << "[WARN] FoxNet: unknown packet type [" << data->type << "]" << std::endl;
                return;
            }

            packetMap[data->type](sock, data);
        }
    
    public:
        FoxClient(std::string ad, uint16_t p): address(ad), port(p) {
            struct sockaddr_in serv_addr;
            SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
            if (SOCKETINVALID(sock)) {
                std::cerr << "[FATAL] FoxNet: socket failed" << std::endl; 
                exit(EXIT_FAILURE); 
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);

            // convert IPv4 and IPv6 addresses from text to binary form (windows is very annoying)
#ifdef _WIN32
            if(InetPtonW(AF_INET, address.c_str(), &serv_addr.sin_addr) != 1) {
#else
            if(inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr) != 1) {
#endif
                std::cerr << "[FATAL] FoxNet: Invalid address" << std::endl; 
                exit(EXIT_FAILURE);
            }

            // Connect to server
            if (SOCKETERROR(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))) {
                std::cerr << "[FATAL] FoxNet: connection failed" << std::endl; 
                exit(EXIT_FAILURE);
            }

            // set socket to non-blocking
#ifdef _WIN32
            unsigned long mode = 1;
            if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
            if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                std::cerr << "[FATAL] FoxNet: fcntl failed" << std::endl; 
                exit(EXIT_FAILURE); 
            }
            
            // sets up connection
            connection = FoxConnection(sock);
        }

        bool isAlive() {
            return connection.isAlive();
        }

        void setPacket(uint16_t ID, FoxPacketHandler handler) {
            packetMap[ID] = handler;
        }

        void step() {
            if (connection.isAlive()) {
                FoxPacket* packet = connection.step();
                if (packet != NULL) {
                    packetHandler(&connection, packet);
                    free(packet->data);
                    delete packet;
                }
            }
        }
    };
}

#endif // hey don't fuck around, your time gets shorter each day. you'll end up sorry-i know