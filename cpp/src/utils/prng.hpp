#pragma once

#include <stdint.h>
#include <time.h>
#include <xxh3.h>

typedef struct ubiquity_prng_s {
    uint64_t seed;
    uint64_t counter;
} ubiquity_prng_t;

static inline void ubiquity_prng_seed(
        ubiquity_prng_t *const prng
        )
{
    struct timespec ts;
    uint64_t prng_seed = 0;
    for (size_t iteration = 0; iteration < 0x1000; ++iteration) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        prng_seed = XXH3_64bits_withSeed(&ts.tv_nsec, sizeof(ts.tv_nsec), prng_seed);
    }
    prng->seed = prng_seed;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    prng->counter = XXH3_64bits_withSeed(&ts.tv_nsec, sizeof(ts.tv_nsec), prng_seed);
}

static inline uint64_t ubiquity_prng_generate(
        ubiquity_prng_t *const prng
        )
{
    ++prng->counter;
    return XXH3_64bits_withSeed(&prng->counter, sizeof(prng->counter), prng->seed);
}

static inline uint64_t ubiquity_prng_generate_once(void)
{
    ubiquity_prng_t prng;
    ubiquity_prng_seed(&prng);
    return ubiquity_prng_generate(&prng);
}
