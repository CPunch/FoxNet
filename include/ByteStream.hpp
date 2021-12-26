#pragma once

#include <cstdlib>
#include <cstdint>
#include <climits>
#include <vector>

#define BSTREAM_RESERVED 64

namespace FoxNet {
    typedef unsigned char Byte;

    /*
     * Variable-Length Array
     *
     *  This is to combat some MSVC ugly-ness and allow us to cleanly define short-lived arrays with variable length
     * without having to remember to clean it up :D
     */
    template<typename T>
    class VLA {
    public:
        T *buf;
        size_t sz;

        VLA(size_t s) {
            buf = new T[s];
        }

        ~VLA() {
            delete[] buf;
        }

        T& operator[](std::size_t i) {
            return buf[i];
        }

        const T& operator[](std::size_t i) const {
            return buf[i];
        } 
    };

    /*
     * ByteStream
     */
    class ByteStream {
    protected:
        std::vector<Byte> inBuffer; // all read operations operate on this buffer
        std::vector<Byte> outBuffer; // all write operations operate on this buffer
        bool flipEndian = false;

        void rawWriteIn(Byte *in, size_t sz); // write to the in buffer

    public:
        ByteStream(void);

        std::vector<Byte>& getOutBuffer(void);
        std::vector<Byte>& getInBuffer(void);
        void flushOut(void); // clears the out (write()) buffer
        void flushIn(void); // clears the in (read()) buffer
        size_t sizeOut(void); // size of the out (write()) buffer
        size_t sizeIn(void); // size of the in (read()) buffer

        // if set to true, integers read and written will be automatically flipped to the opposite endian-ness
        // note: this only has an effect if the integers are written or read using writeInt() or readInt()
        void setFlipEndian(bool);

        virtual bool readBytes(Byte *out, size_t sz);
        virtual void writeBytes(Byte *in, size_t sz);
        bool patchBytes(Byte *in, size_t sz, size_t indx);

        inline void writeByte(Byte in) {
            writeBytes(&in, 1);
        }

        inline bool readByte(Byte &out) {
            return readBytes(&out, 1);
        }

        template <typename T>
        bool readInt(T& data) {
            bool result;
            if (flipEndian) {
                union {
                    T u;
                    Byte u8[sizeof(T)];
                } source, dest;

                // read bytes into union to be swapped
                result = readBytes(source.u8, sizeof(T));

                // copy source to dest, flipping endian
                for (size_t k = 0; k < sizeof(T); k++)
                    dest.u8[k] = source.u8[sizeof(T) - k - 1];

                data = dest.u;
            } else {
                // just read the data straight
                result = readBytes((Byte*)&data, sizeof(T));
            }

            return result;
        }

        template <typename T>
        void writeInt(const T data) {
            if (flipEndian) {
                union {
                    T u;
                    Byte u8[sizeof(T)];
                } source, dest;

                // copy uint into union to be swapped
                source.u = data;

                // copy source to dest, flipping endian
                for (size_t k = 0; k < sizeof(T); k++)
                    dest.u8[k] = source.u8[sizeof(T) - k - 1];

                // write the result
                writeBytes(dest.u8, sizeof(T));
            } else {
                // just write the data straight
                writeBytes((Byte*)&data, sizeof(T));
            }
        }

        template <typename T>
        bool patchInt(const T data, size_t indx) {
            bool result;
            if (flipEndian) {
                union {
                    T u;
                    Byte u8[sizeof(T)];
                } source, dest;

                // copy uint into union to be swapped
                source.u = data;

                // copy source to dest, flipping endian
                for (size_t k = 0; k < sizeof(T); k++)
                    dest.u8[k] = source.u8[sizeof(T) - k - 1];

                // patch the result
                result = patchBytes(dest.u8, sizeof(T), indx);
            } else {
                // just patch the data straight
                result = patchBytes((Byte*)&data, sizeof(T), indx);
            }

            return result;
        }

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