// load_balancer.hpp
#pragma once
#include <map>
#include <memory>
#include "node_client.hpp"
#include "shared_state_store.hpp"

// Selects the best node from targeted_nodes using weighted rendezvous hashing
status_code_t send_message_with_rendezvous(
        const node_client_map_t node_clients,
        const ip_set_t *const targeted_nodes,
        const int64_t balance_key,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size
        ) noexcept;

// Store-aware wrapper — loads snapshot then routes
status_code_t shared_send_message_with_rendezvous(
        shared_state_store_t *const store,
        const ip_set_t *const targeted_nodes,
        const int64_t balance_key,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size
        ) noexcept;