#include "ByteStream.hpp"

using namespace FoxNet;

// constructors
ByteStream::ByteStream() {
    inBuffer.reserve(BSTREAM_RESERVED);
    outBuffer.reserve(BSTREAM_RESERVED);
}

void ByteStream::rawWriteIn(Byte *in, size_t sz) {
    inBuffer.insert(inBuffer.end(), &in[0], &in[sz]);
}

std::vector<Byte>& ByteStream::getOutBuffer() {
    return outBuffer;
}

std::vector<Byte>& ByteStream::getInBuffer() {
    return inBuffer;
}

void ByteStream::flushOut() {
    outBuffer.clear();
    outBuffer.reserve(BSTREAM_RESERVED);
}

void ByteStream::flushIn() {
    inBuffer.clear();
    inBuffer.reserve(BSTREAM_RESERVED);
}

size_t ByteStream::sizeOut() {
    return outBuffer.size();
}

size_t ByteStream::sizeIn() {
    return inBuffer.size();
}

void ByteStream::setFlipEndian(bool _e) {
    flipEndian = _e;
}

bool ByteStream::readBytes(Byte *out, size_t sz) {
    // make sure we can actually read that data :P
    if (inBuffer.size() < sz)
        return false;

    std::copy(inBuffer.begin(), inBuffer.begin() + sz, out);
    inBuffer.erase(inBuffer.begin(), inBuffer.begin() + sz);

    return true;
}

void ByteStream::writeBytes(Byte *in, size_t sz) {
    outBuffer.insert(outBuffer.end(), &in[0], &in[sz]);
}

bool ByteStream::patchBytes(Byte *in, size_t sz, size_t indx) {
    // sanity check
    if (indx + 1 > outBuffer.size())
        return false;

    std::copy(in, in + sz, outBuffer.begin() + indx);
    return false;
}