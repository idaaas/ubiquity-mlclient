#pragma once 

#include <memory>

#include "common/common.hpp"

#include "stdbool.h"

typedef struct connection_s connection_t;
typedef std::shared_ptr<connection_t> connection_ref_t;

/*
 * Called by ubiquity.
 */

bool connection_initialize_module(
        ) noexcept;

void connection_close_module(
        ) noexcept;

bool connection_set_directory(
        const char *const directory_path
        ) noexcept;

/*
 * Called by node client.
 */

void connection_init(
        connection_t *const connection,
        const char *const ip_string,
        const client_session_id_t client_session_id
        ) noexcept;

connection_ref_t connection_create(
        const char *const ip_string,
        const client_session_id_t client_session_id
        ) noexcept;

void connection_delete(
        connection_t *const connection
        ) noexcept;

bool connection_ensure_ready(
        connection_t *const connection
        ) noexcept;

void connection_poll_message(
        connection_t *const connection,
        const stream_id_t stream_id,
        uint8_t *const buffer,
        size_t *message_length
        ) noexcept;

void connection_send_message(
        connection_t *const connection,
        const stream_id_t stream_id,
        uint8_t *const buffer,
        const size_t message_size,
        status_code_t *const status_code
        ) noexcept;

bool connection_is_ready(
        connection_t *const connection
        ) noexcept;

