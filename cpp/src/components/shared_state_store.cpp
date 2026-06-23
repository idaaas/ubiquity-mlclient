#include "shared_state_store.hpp"
#include <cstring>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath> 

typedef struct shared_state_store_s {
    // node clients 
    node_client_map_ref_t  node_clients_active;

    // model infos
    model_info_map_ref_t   models_active;
} shared_state_store_t;

shared_state_store_ref_t shared_state_store_create() 
{
    shared_state_store_t* store = new (std::nothrow) shared_state_store_t{};
    if(!store) {
        return nullptr;
    }

    // initialize with empty maps
    store->node_clients_active = std::make_shared<node_client_map_t>();
    store->models_active = std::make_shared<model_info_map_t>();

    return std::shared_ptr<shared_state_store_s>(store);
}

void shared_state_store_clear(
        shared_state_store_t* store)
{
    std::atomic_store(&store->node_clients_active, std::make_shared<node_client_map_t>());
    std::atomic_store(&store->models_active, std::make_shared<model_info_map_t>());
}

// reader 
node_client_map_ref_t store_get_node_clients(
        shared_state_store_t* store) 
{
    return std::atomic_load(&store->node_clients_active);
}

node_client_ref_t store_get_node_client(
        shared_state_store_t* store,
        ip4_t                 key)
{
    node_client_map_ref_t snap = std::atomic_load(&store->node_clients_active);

    auto it = snap->find(key);
    if (it == snap->end()) {
        return nullptr;
    }
    
    return it->second; // Returns a copy (value), safely pinning the node's lifetime
}

// writer (COW + swap) 
void store_upsert_node_client(
        shared_state_store_t* store,
        ip4_t                 key,
        node_client_ref_t     client)
{
    // Step 1: Load the current snapshot
    node_client_map_ref_t current_snap = std::atomic_load(&store->node_clients_active); // Count is safely +1.

    // Step 2: Perform the map duplication 
    node_client_map_ref_t next = std::make_shared<node_client_map_t>(*current_snap);

    // Step 3: Add client to the new map 
    (*next)[key] = client;

    // Step 4: atomic swap
    std::atomic_store(&store->node_clients_active, next);
}

void store_remove_node_client(
        shared_state_store_t* store,
        ip4_t                 key)
{
    // Step 1: Load the current snapshot
    node_client_map_ref_t current = std::atomic_load(&store->node_clients_active);

    if (current->find(key) == current->end()) {
        return;
    }

    // Step 2: Duplicate the map on the heap
    node_client_map_ref_t next = std::make_shared<node_client_map_t>(*current);

    // Step 3: Remove node from the copy
    next->erase(key);

    // Step 4: atomic swap
    std::atomic_store(&store->node_clients_active, next);
}

// reader
model_info_ref_t store_get_model_info(
        shared_state_store_t* store,
        model_id_t            key)
{
    model_info_map_ref_t snap = atomic_load(&store->models_active);

    auto it = snap->find(key);
    if (it == snap->end()) {
        return nullptr;
    }
    
    return it->second;
}

// writer
void store_upsert_model_info(
        shared_state_store_t* store,
        model_id_t            key,
        model_info_ref_t      info)
{
    // Step 1: Load the current snapshot
    model_info_map_ref_t current_snap = std::atomic_load(&store->models_active);

    // Step 2: Perform the map duplication
    model_info_map_ref_t next = std::make_shared<model_info_map_t>(*current_snap);

    // Step 3: Insert the mdel info to the map (transfert ownership of the shared_ptr)
    (*next)[key] = std::move(info);

    // Step 4:atomic swap
    std::atomic_store(&store->models_active, next);
}

void store_remove_model_info(
        shared_state_store_t* store,
        model_id_t            key)
{
    // Step 1: Load the current snapshot
    model_info_map_ref_t current = std::atomic_load(&store->models_active);

    if (current->find(key) == current->end()) {
        return;
    }

    // Step 2: Duplicate the map on the heap
    model_info_map_ref_t next = std::make_shared<model_info_map_t>(*current);

    // Step 3: Mutate private copy
    next->erase(key);

    // Step 4:  atomic swap
    std::atomic_store(&store->models_active, next);
}

/*
 *  POLL a message from active node_clients 
 */

size_t poll_message(
        shared_state_store_t *const store,
        const stream_id_t stream_id,
        uint8_t *const buffer
        ) noexcept
{
    node_client_map_ref_t node_clients = store_get_node_clients(store);
    for (const auto& entry : *node_clients) {
        // ip4_t ip = entry.first;
        node_client_ref_t node_client = entry.second;

        if(node_client) {
            // Poll message from the node client.
            size_t message_length = 0;
            node_client_poll_message(
                    node_client.get(),
                    stream_id,
                    buffer,
                    &message_length
                    );
            if (message_length > 0) {
                return message_length;
            }
        }
    }
    return 0;
}
