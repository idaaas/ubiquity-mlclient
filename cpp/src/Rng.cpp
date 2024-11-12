#include "debug.hpp"
#include "Rng.hpp"
#include <chrono>

Rng::Mode Rng::mode = NOT_SEEDED;
std::mt19937_64 Rng::gen;
std::ranlux48 Rng::rl;

void Rng::seed(void) noexcept {
    try {
        gen.seed(std::random_device{}());
        mode = DEVICE_SEEDED;
    } catch (const std::exception &e) {
        DBG("Random device not found, rng will be time seeded");
        auto now = std::chrono::high_resolution_clock::now();
        auto timeSeed = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        std::ranlux48 rl;
        rl.seed(timeSeed);
        mode = TIME_SEEDED;
    }
}

uint64_t Rng::generateUint64(void) noexcept {
    if (mode == NOT_SEEDED) seed();

    if (mode == DEVICE_SEEDED) {
        std::uniform_int_distribution<uint64_t> dist{0, UINT64_MAX};
        return dist(gen);
    }
        
    return rl();
}
