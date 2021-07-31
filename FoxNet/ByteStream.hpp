#pragma once

#include <cstdlib>
#include <cstdint>
#include <vector>

#define BSTREAM_RESERVED 64

namespace FoxNet {
    typedef unsigned char Byte;

    /*
     * ByteStream, a simple first-in/first-out buffer
     */
    class ByteStream {
    private:
        std::vector<Byte> buffer;

    public:
        ByteStream();
        ByteStream(std::vector<Byte>& raw);

        std::vector<Byte>& getBuffer();
        void flush(); // clears the buffer
        size_t size();

        void readBytes(Byte *out, size_t sz);
        void writeBytes(Byte *in, size_t sz);

        template<typename T>
        inline void writeData(const T& data) {
            writeBytes((Byte*)(&data), sizeof(T));
        }

        template<typename T>
        inline void readData(T& data) {
            readBytes((Byte*)(&data), sizeof(T));
        }
    };
}