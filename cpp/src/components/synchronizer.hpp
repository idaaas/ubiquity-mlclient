#pragma once
#include <memory>

#include "common/common.hpp"
#include "utils/logger.hpp"

typedef struct synchronizer_s synchronizer_t;
typedef struct model_info_s model_info_t;
typedef struct shared_state_store_s shared_state_store_t;
typedef struct std::shared_ptr<shared_state_store_s> shared_state_store_ref_t;

/*
    Called by the cluster client.
*/

synchronizer_t *synchronizer_create(
        const char *const cluster_id,
        shared_state_store_ref_t store
        ) noexcept;

void synchronizer_delete(
        synchronizer_t *const synchronizer
        ) noexcept;

bool synchronizer_store_initial_topology(
        synchronizer_t *const synchronizer,
        cluster_info_t *const cluster_info
        ) noexcept;

void synchronizer_use_initial_topology(
        synchronizer_t *const synchronizer
        ) noexcept;

size_t synchronizer_invoke(
        synchronizer_t *const synchronizer
        ) noexcept;

/*
    Called internally and by component testing.
*/

bool synchronizer_apply_block_update(
        synchronizer_t *const synchronizer,
        const uint8_t block_type,
        const uint16_t block_index,
        const uint16_t new_block_size,
        const void *const new_block_content,
        const block_hash_t *const block_hash
        ) noexcept;
