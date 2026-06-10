#pragma once

#include "common/common.hpp"

typedef struct backoff_idle_strategy_s {
    // conf params for the reset
    int64_t max_spins;
    int64_t max_yields;
    int64_t min_park_ns;
    int64_t max_park_ns;
    // current strategy state 
    int64_t spins;
    int64_t yields;
    int64_t park_ns;
    // Keep track of last work count 
    int work_count;
} backoff_idle_strategy_t;


void backoff_idle_strategy_init(
        backoff_idle_strategy_t* idle_strategy
        ) noexcept;

void backoff_idle_strategy_set_work_count(
        backoff_idle_strategy_t* idle_strategy,
        int work_count 
        ) noexcept;

void backoff_idle_strategy_idle(
        backoff_idle_strategy_t* idle_strategy 
        ) noexcept;
