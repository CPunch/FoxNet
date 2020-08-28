/*  Free Open Xross Network

    This file is a single-header-library for a basic TCP cross-platform event-driven network stack. 
*/

#ifndef _FOXNET_PROTOCOL
#define _FOXNET_PROTOCOL

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
    typedef void buffer_t;
    #define FOXERRNO errno
    #define FOXEWOULDBLOCK EWOULDBLOCK
    #define SOCKETINVALID(x) (x < 0)
    #define SOCKETERROR(x) (x == -1)
#else
// windows (UNTESTED)
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    typedef char buffer_t;
    #define FOXERRNO WSAGetLastError()
    #define FOXEWOULDBLOCK WSAEWOULDBLOCK
    #define SOCKETINVALID(x) (x == INVALID_SOCKET)
    #define SOCKETERROR(x) (x == SOCKET_ERROR)
#endif
#include <fcntl.h>
#include <csignal>

#include <string>
#include <mutex>
#include <queue>
#include <list>
#include <map>
#define MAX_PACKETSIZE 1024 * 8

#define DEBUGLOG(x) x

namespace FoxNet {
    // INIT/QUIT (mainly for windows bc windows SUCKKKKKSSSSSSS)
    inline void init() {
        signal(SIGPIPE, SIG_IGN);
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
            DEBUGLOG(std::cerr << "[FATAL] FoxNet: calloc failed!" << std::endl);
            exit(EXIT_FAILURE);
        }
        return buf;
    }

    namespace FoxObfuscaton {
        static const int defaultKey = 0x34;

        // xors data with default static key
        void xorData(void* buf, size_t sz, int key) {
            for (uint8_t* b = (uint8_t*)buf; b < (uint8_t*)buf + sz; b++) {
                *b ^= key;
            }
        }

        void encodeData(void* buf, size_t sz) {
            xorData(buf, sz, defaultKey);
        }

        void decodeData(void* buf, size_t sz) {
            xorData(buf, sz, defaultKey);
        }
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
        bool alive = true;

        // guaranteed to send all the data
        bool sendData(void* data, int size) {
            int sentBytes = 0;

            while (sentBytes < size) {
                int sent = write(sock, (buffer_t*)((uint8_t*)data + sentBytes), size - sentBytes); // no flags defined

                if (SOCKETERROR(sent)) {
                    return false; // error occured while sending bytes
                }
                sentBytes += sent;
            }

            return true; // it worked!
        }

        // not guaranteed to recieve all the data! check the return value for how much data was actually recieved!!
        int recieveData(SOCKET s, buffer_t* buf, size_t sz) {
            int recved = read(s, buf, sz);

            if (SOCKETERROR(recved)) {
                if (FOXERRNO == FOXEWOULDBLOCK) {
                    // try again
                    return -1;
                }

                // a serious socket error occured, close the connection!
                kill();
                return -1;
            }

            // we got the data successfully!
            return recved;
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

        bool sendPacket(FoxPacket& data) {
            uint32_t pSize = htonl(data.size + sizeof(uint16_t));
            uint16_t pType = htons(data.type);

            // we allocate a tmpBuf so we can obfuscate the bytes with FoxObfuscate::encodeData()
            void* tmpBuf = xmalloc(data.size + sizeof(uint16_t));
            // copy packet ID
            memcpy(tmpBuf, (void*)&pType, sizeof(uint16_t));
            // copy packet data to tmpBuf after the packet ID
            memcpy((uint8_t*)tmpBuf + sizeof(uint16_t), data.data, data.size);

            // encode the data
            FoxObfuscaton::encodeData(tmpBuf, data.size + sizeof(uint16_t));

            // send size of the pack (includes type)
            if (!sendData((void*)&pSize, sizeof(uint32_t))) { // failed to send the data
                kill();
                free(tmpBuf);
                return false;
            }

            // send obfuscated packet
            if (!sendData(tmpBuf, data.size + sizeof(uint16_t))) {
                kill();
                free(tmpBuf);
                return false;
            }

            // free tmpBuf
            free(tmpBuf);
            return true;
        }

        FoxPacket* step() {
            if (recvSize == 0) {
                // read packet size
                int recved;

                // set recved & if recieveData failed, return NULL
                if ((recved = recieveData(sock, (buffer_t*)&recvSize, sizeof(uint32_t))) == -1)
                    return NULL;
                
                recvSize = ntohl(recvSize);


                if (recvSize > MAX_PACKETSIZE) { // possible DDoS attack, abort connection
                    kill();
                    return NULL;
                }

                if (recvSize > 0) {
                    std::cout << "got packet size: " << recvSize << std::endl;
                    recvBuffer = xmalloc(recvSize);
                }
            }

            if (recvSize > 0 && recvIndex < recvSize) {
                int recved = recieveData(sock, (buffer_t*)((uint8_t*)recvBuffer + recvIndex), recvSize);

                if (recved == -1) 
                    return NULL;
                else
                    recvIndex += recved;
            }

            // check if we've finished reading the packet!
            if (recvSize > 0 && recvIndex - recvSize == 0) {
                // unobfuscate the data
                FoxObfuscaton::decodeData(recvBuffer, recvSize);

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
        bool active = true;

        void packetHandler(FoxConnection* sock, FoxPacket* data) {
            if (packetMap.find(data->type) == packetMap.end()) {
                DEBUGLOG(std::cerr << "[WARN] FoxNet: unknown packet type [" << data->type << "]" << std::endl);
                return;
            }

            packetMap[data->type](sock, data);
        }

    public:
        FoxServer(uint16_t p): port(p) {
            // create socket file descriptor 
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (SOCKETINVALID(sock)) { 
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: socket failed" << std::endl); 
                exit(EXIT_FAILURE); 
            }

            // attach socket to the port
            int opt = 1;
#ifdef _WIN32
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) { 
#else
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) { 
#endif
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: setsockopt failed" << std::endl); 
                exit(EXIT_FAILURE); 
            } 
            address.sin_family = AF_INET; 
            address.sin_addr.s_addr = INADDR_ANY; 
            address.sin_port = htons(port); 

            addressSize = sizeof(address);

            // Bind to the port
            if (SOCKETERROR(bind(sock, (struct sockaddr *)&address, addressSize))) { 
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: bind failed" << std::endl); 
                exit(EXIT_FAILURE); 
            }

            if (SOCKETERROR(listen(sock, SOMAXCONN))) {
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: listen failed" << std::endl); 
                exit(EXIT_FAILURE); 
            }

            // set server listener to non-blocking
#ifdef _WIN32
            unsigned long mode = 1;
            if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
            if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: fcntl failed" << std::endl); 
                exit(EXIT_FAILURE); 
            }
        }

        ~FoxServer() {
            kill();
        }

        void kill() {
            if (!active)
                return;
            
            active = false;

            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends
#ifdef _WIN32
            shutdown(sock, SD_BOTH);
            closesocket(sock);
#else
            shutdown(sock, SHUT_RDWR);
            close(sock);
#endif

            // kill all connections
            for (FoxConnection* con : connections) {
                con->kill();
                delete con;
            }

            connections.clear();
        }

        void setPacket(uint16_t ID, FoxPacketHandler handler) {
            packetMap[ID] = handler;
        }

        // will automatically free FoxPacket when done
        void sendPacketToAll(FoxPacket data) {
            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends

            for (FoxConnection* con: connections) {
                if (con->isAlive()) {
                    con->sendPacket(data);
                }
                // let step() deal with all the dead connections /shrug
            }
        }

        bool sendPacket(FoxConnection* sock, FoxPacket data) {
            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends

            if (!sock->isAlive())
                return false;

            sock->sendPacket(data);

            return true;
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
                    DEBUGLOG(std::cerr << "[FATAL] FoxNet: fcntl failed on new connection" << std::endl); 
                    exit(EXIT_FAILURE); 
                }

                DEBUGLOG(std::cout << "New connection! " << inet_ntoa(address.sin_addr) << std::endl);

                // add connection to list!
                FoxConnection* tmp = new FoxConnection(newConnection);
                connections.push_back(tmp);
                onConnect(tmp);
            }

            // for each active connection, step()
            std::list<FoxConnection*>::iterator i = connections.begin();
            while (i != connections.end()) {
                FoxConnection* current = *i;

                if (!current->isAlive()) { // remove inactive connections
                    onDisconnect(current);
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
            while (active) {
                step();
#ifdef _WIN32
                Sleep(0);
#else
                sleep(0); // lets the OS run other threads so we're not hitting 100% CPU
#endif
            }
        }

        // events meant to be overwritten
        virtual void onConnect(FoxConnection* sock) {}
        virtual void onDisconnect(FoxConnection* sock) {} 
    };

    class FoxClient {
    private:
        std::map<uint16_t, FoxPacketHandler> packetMap;
        std::mutex activeCritical;

        FoxConnection connection;
        std::string address;
        uint16_t port;

        void packetHandler(FoxConnection* sock, FoxPacket* data) {
            if (packetMap.find(data->type) == packetMap.end()) {
                DEBUGLOG(std::cerr << "[WARN] FoxNet: unknown packet type [" << data->type << "]" << std::endl);
                return;
            }

            packetMap[data->type](sock, data);
        }
    
    public:
        FoxClient(std::string ad, uint16_t p): address(ad), port(p) {
            struct sockaddr_in serv_addr;
            SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
            if (SOCKETINVALID(sock)) {
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: socket failed" << std::endl); 
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
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: Invalid address" << std::endl); 
                exit(EXIT_FAILURE);
            }

            // Connect to server
            if (SOCKETERROR(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))) {
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: connection failed" << std::endl); 
                exit(EXIT_FAILURE);
            }

            // set socket to non-blocking
#ifdef _WIN32
            unsigned long mode = 1;
            if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
            if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                DEBUGLOG(std::cerr << "[FATAL] FoxNet: fcntl failed" << std::endl); 
                exit(EXIT_FAILURE); 
            }
            
            // sets up connection
            connection = FoxConnection(sock);
        }

        bool isAlive() {
            return connection.isAlive();
        }

        void kill() {
            std::cout << "dead >:(" << std::endl;
            connection.kill();
        }

        void setPacket(uint16_t ID, FoxPacketHandler handler) {
            packetMap[ID] = handler;
        }

        void sendPacket(FoxPacket data) {
            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends

            connection.sendPacket(data);
        }

        void step() {
            std::lock_guard<std::mutex> lock(activeCritical); // the lock will be removed when the function ends

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

#endif // hey don't mess around, your time gets shorter each day. you'll end up sorry-i know
