/*
 * Backoff idle strategy inspired by Aeron's aeron_agent idle strategy.
 * https://github.com/real-logic/aeron
 * Original work Copyright 2014-2025 Real Logic Limited, Apache 2.0 License.
 */

#include "idle_strategy.hpp"
#include "common/common.hpp"

#include <sched.h>
#include <time.h>

#define BACKOFF_MAX_SPINS (10)
#define BACKOFF_MAX_YIELDS (20)
#define BACKOFF_MIN_PARK_PERIOD_NS (1000LL)
#define BACKOFF_MAX_PARK_PERIOD_NS (1 * 1000 * 1000LL)

void backoff_idle_strategy_init(
        backoff_idle_strategy_t *strategy
        )noexcept
{
    strategy->max_spins   = BACKOFF_MAX_SPINS;
    strategy->max_yields  = BACKOFF_MAX_YIELDS;
    strategy->min_park_ns = BACKOFF_MIN_PARK_PERIOD_NS;
    strategy->max_park_ns = BACKOFF_MAX_PARK_PERIOD_NS;

    strategy->spins       = 0;
    strategy->yields      = 0;
    strategy->park_ns     = strategy->min_park_ns;
    strategy->work_count  = 0;
}

static inline void proc_yield(
        ) noexcept
{
    // architecure specific yield
#if defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("yield" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}        

void backoff_idle_strategy_idle(
        backoff_idle_strategy_t* idle_strategy 
        ) noexcept 
{
    if (idle_strategy->work_count > 0) {
        // Reset state 
        idle_strategy->spins   = 0;
        idle_strategy->yields  = 0;
        idle_strategy->park_ns = idle_strategy->min_park_ns;
        return;
    }
    if (idle_strategy->spins < idle_strategy->max_spins) {
        // Busy spin 
        proc_yield();
        ++idle_strategy->spins;        
    } 
    else if (idle_strategy->yields < idle_strategy->max_yields) {
        // Yield 
        sched_yield();
        ++idle_strategy->yields;        
    }
    else {
        // Backoff 
        struct timespec ts = {0, idle_strategy->park_ns};
        nanosleep(&ts, NULL);
        // exponentially increase parking duration
        idle_strategy->park_ns = 
            idle_strategy->park_ns * 2 > idle_strategy->max_park_ns ?
                idle_strategy->max_park_ns : idle_strategy->park_ns * 2;
    }
}

void backoff_idle_strategy_set_work_count(
        backoff_idle_strategy_t* idle_strategy,
        int work_count 
        ) noexcept 
{
    idle_strategy->work_count = work_count;
}

