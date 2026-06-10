// shared_state_store.h
#pragma once
#include <map>
#include <memory>
#include "node_client.hpp"
#include "common/model_info.hpp"

typedef std::map<ip4_t, node_client_ref_t> node_client_map_t;

typedef std::map<model_id_t, model_info_ref_t> model_info_map_t;

typedef std::shared_ptr<node_client_map_t> node_client_map_ref_t;

typedef std::shared_ptr<model_info_map_t> model_info_map_ref_t;

typedef struct shared_state_store_s shared_state_store_t;

typedef struct std::shared_ptr<shared_state_store_s> shared_state_store_ref_t;

shared_state_store_ref_t shared_state_store_create();

void shared_state_store_clear(
        shared_state_store_t* store
        );

node_client_map_ref_t store_get_node_clients(
        shared_state_store_t* store);

node_client_ref_t store_get_node_client(
        shared_state_store_t* store,
        ip4_t                 key);

void store_upsert_node_client(
        shared_state_store_t* store,
        ip4_t                 key,
        node_client_ref_t     client);

void store_remove_node_client(
        shared_state_store_t* store,
        ip4_t                 key);

model_info_ref_t store_get_model_info(
        shared_state_store_t* store,
        model_id_t            key);

void store_upsert_model_info(
        shared_state_store_t* store,
        model_id_t            key,
        model_info_ref_t      info);

void store_remove_model_info(
        shared_state_store_t* store,
        model_id_t            key);


size_t poll_message(
        shared_state_store_t *const store,
        const stream_id_t stream_id,
        uint8_t *const buffer
        ) noexcept;
