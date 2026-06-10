#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include <stdint.h>

#define NANO_SECONDS_PER_SECONDS          1000000000ll
#define NANO_SECONDS_PER_MILLISECONDS     1000000ll
#define NANO_SECONDS_PER_MICROSECONDS     1000ll

#define UBIQUITY_TIME_SECONDS(n) (n * NANO_SECONDS_PER_SECONDS)
#define UBIQUITY_TIME_MICROSECONDS(n) (n * NANO_SECONDS_PER_MICROSECONDS)
#define UBIQUITY_TIME_MILLISECONDS(n) (n * NANO_SECONDS_PER_MILLISECONDS)

typedef int64_t ubiquity_time_t;

static inline ubiquity_time_t ubiquity_time_get_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * NANO_SECONDS_PER_SECONDS + ts.tv_nsec;
}

static inline ubiquity_time_t ubiquity_time_get_from_now(
        const ubiquity_time_t period
        )
{
    return period + ubiquity_time_get_now();
}

static inline ubiquity_time_t ubiquity_time_is_older(
        const ubiquity_time_t old_time,
        const ubiquity_time_t new_time
        )
{
    return old_time < new_time;
}

static inline ubiquity_time_t ubiquity_time_is_past(
        const ubiquity_time_t old_time
        )
{
    return ubiquity_time_is_older(
            old_time,
            ubiquity_time_get_now()
            );
}

static inline ubiquity_time_t ubiquity_time_get_period(
        const ubiquity_time_t start_time,
        const ubiquity_time_t end_time
        )
{
    return end_time - start_time;
}

static inline ubiquity_time_t ubiquity_time_get_elapsed_from(
        const ubiquity_time_t start_time
        )
{
    return ubiquity_time_get_period(
            start_time,
            ubiquity_time_get_now()
            );
}

static inline bool ubiquity_time_period_is_elapsed(
        const ubiquity_time_t start_time,
        const ubiquity_time_t period
        )
{
    return period <= ubiquity_time_get_period(
            start_time,
            ubiquity_time_get_now()
            );
}

static inline bool ubiquity_time_period_was_elapsed(
        const ubiquity_time_t start_time,
        const ubiquity_time_t end_time,
        const ubiquity_time_t period
        )
{
    return period <= ubiquity_time_get_period(
            start_time,
            end_time
            );
}

static inline void ubiquity_time_sleep(
        const ubiquity_time_t period
        )
{
    struct timespec ts;
    ts.tv_sec = period / NANO_SECONDS_PER_SECONDS;
    ts.tv_nsec = period % NANO_SECONDS_PER_SECONDS;
    nanosleep(&ts, NULL);
}

#ifdef __cplusplus
}
#endif
