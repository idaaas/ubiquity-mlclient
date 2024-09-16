#pragma once

#ifndef NDEBUG
    #include <iostream>

    #define DBG(message)                        \
        {                                       \
            std::cout << message << std::endl;  \
        }
    #define IGNORE(message)

    #include <Aeron.h>
    void dumpBufferAsHexadecimal(const uint8_t *const buf, const size_t length);
    void dumpBufferAsHexadecimal(const aeron::concurrent::AtomicBuffer &buf, const size_t length);

    #define hexDump(buf, length)                    \
        {                                           \
            dumpBufferAsHexadecimal(buf, length);   \
        }
#else
    #define DBG(message)
    #define hexDump(buf, length)
    #define IGNORE(message)
#endif

