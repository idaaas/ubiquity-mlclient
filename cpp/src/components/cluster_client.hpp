#pragma once

#include "common/common.hpp"
#include "common/model_info.hpp"
#include "utils/logger.hpp"

#include <string>
#include <thread>

typedef struct cluster_client_s cluster_client_t;
typedef struct synchronizer_s synchronizer_t;
typedef struct client_handle_s client_handle_t;

/*
    Called by ubiquity.
*/

cluster_client_t *cluster_client_create(
        const char *const cluster_id,
        status_code_t *const status_code
        ) noexcept;

client_handle_t *cluster_client_create_handle(
        cluster_client_t *const cluster_client
        ) noexcept;

void cluster_client_force_termination(
        cluster_client_t *const cluster_client
        ) noexcept;

void cluster_client_allow_termination(
        cluster_client_t *const cluster_client
        ) noexcept;

/*
    Called by client handle.
*/

void cluster_client_increment_use_count(
        cluster_client_t *const cluster_client
        ) noexcept;

void cluster_client_decrement_use_count(
        cluster_client_t *const cluster_client
        ) noexcept;

/*
    TODO
    Should be called by ubiquity at some point to allow user to check for cluster connectivity.
*/

bool cluster_client_is_connected(
        cluster_client_t *const cluster_client
        ) noexcept;

