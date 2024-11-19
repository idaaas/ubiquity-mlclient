#ifndef NDEBUG

#include "debug.hpp"

#include <cstdint>
#include <cstdio>

void dumpBufferAsHexadecimal(const uint8_t *const buf, const size_t length)
{
    for (size_t i = 0; i < length / 4; i++) {
        printf("%02x %02x %02x %02x\n",
                buf[i * 4],
                buf[i * 4 + 1],
                buf[i * 4 + 2],
                buf[i * 4 + 3]);
    }
    if (0 != length % 4) {
        for (size_t i = 0; i < length % 4; i++) {
            printf("%02x ", buf[length - (length % 4) + i]);
        }
        printf("\n");
    }
    printf("############\n\n");
}

uint8_t buf[1300];

void dumpBufferAsHexadecimal(const aeron::concurrent::AtomicBuffer &atomicBuffer, const size_t length)
{
    atomicBuffer.getBytes(0, buf, 1300 > length ? length : 1300);
    for (size_t i = 0; i < length / 4; i++) {
        printf("%02x %02x %02x %02x\n",
                buf[i * 4],
                buf[i * 4 + 1],
                buf[i * 4 + 2],
                buf[i * 4 + 3]);
    }
    if (0 != length % 4) {
        for (size_t i = 0; i < length % 4; i++) {
            printf("%02x ", buf[length - (length % 4) + i]);
        }
        printf("\n");
    }
    printf("############\n\n");
}

#endif


