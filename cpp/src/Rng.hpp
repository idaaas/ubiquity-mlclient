#pragma once

#include <random>

class Rng {
    public:
        static void seed(void) noexcept;
        static uint64_t generateUint64(void) noexcept;

    private:
        enum Mode {
            NOT_SEEDED,
            DEVICE_SEEDED,
            TIME_SEEDED
        };

        static Mode mode;
        static std::mt19937_64 gen;
        static std::ranlux48 rl;
};
