#pragma once

#include <memory>
#include "common/common.hpp"

#include "utils/logger.hpp"

#define CONNECTION_EMPTY 0
#define CONNECTION_CREATED 1
#define CONNECTION_UP 2
#define CONNECTION_DOWN 3

typedef struct node_client_s node_client_t;
typedef std::shared_ptr<node_client_t>  node_client_ref_t;
typedef struct connection_s connection_t;
typedef uint64_t computed_node_weight_t;
typedef uint8_t connection_status_t;

node_client_ref_t node_client_create(
        const ip4_t ip,
        const node_weight_t weight) noexcept;

ip4_t node_client_get_ip(
        const node_client_t *const node_client
        ) noexcept;

computed_node_weight_t node_client_get_weight(
        const node_client_t *const node_client
        ) noexcept;

void node_client_set_weight(
        node_client_t *const node_client,
        const node_weight_t weight
        ) noexcept;       

connection_status_t node_client_get_connection_status(
            node_client_t *const node_client
            ) noexcept;

void node_client_delete(
        node_client_t *const node_client
        ) noexcept;

connection_status_t node_client_invoke(
        node_client_t *const node_client
        ) noexcept;

void node_client_send_message(
        node_client_t *const node_client,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size,
        status_code_t *const status_code
        ) noexcept;

void node_client_poll_message(
        node_client_t *const node_client,
        const stream_id_t stream_id,
        uint8_t *const destination_buffer,
        size_t *const message_length
        ) noexcept;
