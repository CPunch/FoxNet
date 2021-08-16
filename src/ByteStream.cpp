#include "ByteStream.hpp"

using namespace FoxNet;

// constructors
ByteStream::ByteStream() {
    buffer.reserve(BSTREAM_RESERVED);
}
ByteStream::ByteStream(std::vector<Byte>& raw): buffer(raw) {}

std::vector<Byte>& ByteStream::getBuffer() {
    return buffer;
}

void ByteStream::flush() {
    buffer.clear();
    buffer.reserve(BSTREAM_RESERVED);
}

size_t ByteStream::size() {
    return buffer.size();
}

void ByteStream::setFlipEndian(bool _e) {
    flipEndian = _e;
}

bool ByteStream::readBytes(Byte *out, size_t sz) {
    // make sure we can actually read that data :P
    if (buffer.size() < sz)
        return false;

    std::copy(buffer.begin(), buffer.begin() + sz, out);
    buffer.erase(buffer.begin(), buffer.begin() + sz);

    return true;
}

void ByteStream::writeBytes(Byte *in, size_t sz) {
    buffer.insert(buffer.end(), &in[0], &in[sz]);
}

bool ByteStream::patchBytes(Byte *in, size_t sz, size_t indx) {
    // sanity check
    if (indx + 1 > buffer.size())
        return false;

    std::copy(in, in + sz, buffer.begin() + indx);
    return false;
}