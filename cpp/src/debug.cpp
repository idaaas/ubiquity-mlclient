#ifndef NDEBUG

#include "debug.hpp"

#include <cstdint>
#include <cstdio>

void dumpBufferAsHexadecimal(const uint8_t *const buf, const size_t length)
{
    for (size_t i = 0; i < length / 4; i++) {
        dprintf(2, "%02x %02x %02x %02x\n", (unsigned int)buf[i * 4], (unsigned int)buf[i * 4 + 1], (unsigned int)buf[i * 4 + 2], (unsigned int)buf[i * 4 + 3]);
    }
    for (size_t i = 0; i < length % 4; i++) {
        dprintf(2, "%02x ", (unsigned int)buf[length - (length % 4) + i]);
    }
    dprintf(2, "############\n\n");
}

uint8_t buf[1300];



void dumpBufferAsHexadecimal(const aeron::concurrent::AtomicBuffer &atomicBuffer, const size_t length)
{
    atomicBuffer.getBytes(0, buf, 1300 > length ? length : 1300);
    for (size_t i = 0; i < length / 4; i++) {
        dprintf(2, "%02x %02x %02x %02x\n", (unsigned int)buf[i * 4], (unsigned int)buf[i * 4 + 1], (unsigned int)buf[i * 4 + 2], (unsigned int)buf[i * 4 + 3]);
    }
    for (size_t i = 0; i < length % 4; i++) {
        dprintf(2, "%02x ", (unsigned int)buf[length - (length % 4) + i]);
    }
    dprintf(2, "############\n\n");
}

#endif


