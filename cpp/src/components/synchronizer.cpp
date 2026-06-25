#include "synchronizer.hpp"
#include "shared_state_store.hpp"
#include "load_balancer.hpp"
#include "node_client.hpp"
#include "common/model_info.hpp"
#include "utils/prng.hpp"
#include "utils/ubiquity_time.h"
#include <xxh3.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <vector>

static const char *get_ip_string(
        const uint32_t ip
        ) noexcept
{
    static char ip_string[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &ip, ip_string, INET_ADDRSTRLEN);
    return ip_string;
}

bool compute_md5(
    void *const destination,
    const void *const source,
    const size_t source_size
) noexcept
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    if (!ctx) return false;

    unsigned int md_len = 0;
    const bool success =
        EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) &&
        EVP_DigestUpdate(ctx, source, source_size) &&
        EVP_DigestFinal_ex(ctx, static_cast<unsigned char *>(destination), &md_len);

    EVP_MD_CTX_destroy(ctx);
    return success;
}

// Give 2 seconds to node clients to connect before asking them for block updates.
#define INITIAL_BLOCK_REQUEST_DELAY UBIQUITY_TIME_SECONDS(2)

#define MAX_BLOCK_REQUEST_PERIOD_SECONDS 15
#define MIN_BLOCK_REQUEST_PERIOD_SECONDS 3

#define INNER_CONCAT(alpha, beta) alpha ## beta
#define CONCAT(alpha, beta) INNER_CONCAT(alpha, beta)

#define DECLARE_BLOCK_STORE(                                                                                           \
        block_type,                                                                                                    \
        block_count                                                                                                    \
        )                                                                                                              \
                                                                                                                       \
typedef struct CONCAT(block_type, _block_store_s) {                                                                    \
    bool successful_update;                                                                                            \
    size_t request_period;                                                                                             \
    ubiquity_time_t next_request_time;                                                                                 \
    root_hash_t root_hash;                                                                                             \
    block_hash_t block_hash_list[block_count];                                                                         \
    size_t block_size_list[block_count];                                                                               \
    CONCAT(block_type, _block_t) block_content_list[block_count];                                                      \
} CONCAT(block_type, _block_store_t);                                                                                  \
                                                                                                                       \
static inline void CONCAT(block_type, _block_store_update_root_hash)(                                                  \
        CONCAT(block_type, _block_store_t) *const block_store                                                          \
        )                                                                                                              \
{                                                                                                                      \
    compute_md5(                                                                                                       \
            &block_store->root_hash,                                                                                   \
            &block_store->block_hash_list,                                                                             \
            sizeof(block_hash_t) * block_count                                                                         \
            );                                                                                                         \
}                                                                                                                      \
                                                                                                                       \
static inline void CONCAT(block_type, _block_store_initialize)(                                                        \
        CONCAT(block_type, _block_store_t) *const block_store                                                          \
        )                                                                                                              \
{                                                                                                                      \
    block_hash_t empty_block_hash;                                                                                     \
    compute_md5(                                                                                                       \
            &empty_block_hash,                                                                                         \
            NULL,                                                                                                      \
            0                                                                                                          \
            );                                                                                                         \
    for (size_t block_index = 0; block_index < block_count; ++block_index) {                                           \
        block_store->block_size_list[block_index] = 0;                                                                 \
        block_store->block_hash_list[block_index] = empty_block_hash;                                                  \
    }                                                                                                                  \
    CONCAT(block_type, _block_store_update_root_hash)(block_store);                                                    \
    block_store->successful_update = true;                                                                             \
    block_store->request_period = MAX_BLOCK_REQUEST_PERIOD_SECONDS;                                                    \
    block_store->next_request_time = ubiquity_time_get_from_now(INITIAL_BLOCK_REQUEST_DELAY);                          \
}                                                                                                                      \
                                                                                                                       \
static inline void CONCAT(block_type, _block_store_update_block)(                                                      \
        CONCAT(block_type, _block_store_t) *const block_store,                                                         \
        const size_t block_index,                                                                                      \
        const void *const new_block_content,                                                                           \
        const size_t new_block_size                                                                                    \
        )                                                                                                              \
{                                                                                                                      \
    memcpy(                                                                                                            \
            block_store->block_content_list + block_index,                                                             \
            new_block_content,                                                                                         \
            new_block_size                                                                                             \
            );                                                                                                         \
                                                                                                                       \
    block_store->block_size_list[block_index] = new_block_size;                                                        \
                                                                                                                       \
    compute_md5(                                                                                                       \
            &block_store->block_hash_list[block_index],                                                                \
            new_block_content,                                                                                         \
            new_block_size                                                                                             \
            );                                                                                                         \
    block_store->successful_update = true;                                                                             \
}                                                                                                                      \
                                                                                                                       \
static inline size_t CONCAT(block_type, _block_store_export_block_hash_list)(                                          \
        CONCAT(block_type, _block_store_t) *const block_store,                                                         \
        void *const destination                                                                                        \
        )                                                                                                              \
{                                                                                                                      \
    memcpy(                                                                                                            \
            destination,                                                                                               \
            block_store->block_hash_list,                                                                              \
            sizeof(block_hash_t) * block_count                                                                         \
            );                                                                                                         \
    return sizeof(block_hash_t) * block_count;                                                                         \
}                                                                                                                      \

typedef struct topology_element_s {
    uint32_t ip;
    float weight;
} __attribute__((packed)) topology_element_t;

typedef struct topology_block_s { 
    topology_element_t element_list[MAXIMUM_NODE_COUNT];
} topology_block_t;

DECLARE_BLOCK_STORE(
        topology,
        TOPOLOGY_BLOCK_COUNT
        )

typedef struct model_sharding_element_s {
    uint32_t ip;
    uint64_t model_id;
} __attribute__((packed)) model_sharding_element_t;

typedef struct model_sharding_block_s {
    model_sharding_element_t element_list[MAXIMUM_MODEL_COUNT_PER_SHARDING_BLOCK];
} model_sharding_block_t;

DECLARE_BLOCK_STORE(
        model_sharding,
        MODEL_SHARDING_BLOCK_COUNT
        )

typedef struct model_metadata_element_s {
    uint64_t model_id;
    uint16_t schema_version;
    int16_t application_id;
    uint16_t feedback_count;
    uint16_t feature_type_count;
    feedback_id_list_t feedback_id_list;
    feedback_delay_list_t feedback_delay_list;
    feature_type_list_t feature_type_list;
} __attribute__((packed)) model_metadata_element_t;

typedef struct model_metadata_block_s {
    model_metadata_element_t element_list[MAXIMUM_MODEL_COUNT_PER_METADATA_BLOCK];
} model_metadata_block_t;

DECLARE_BLOCK_STORE(
        model_metadata,
        MODEL_METADATA_BLOCK_COUNT
        )

typedef struct synchronizer_s {
    char cluster_id[CLUSTER_ID_MAX_LENGTH];
    ubiquity_prng_t prng;
    ubiquity_logger_t logger;

    bool is_terminating;

    // Buffer used to receive synchronization messages from the cluster.
    uint8_t message_buffer[MAXIMUM_MESSAGE_SIZE];
    size_t message_size;

    // Block stores.
    topology_block_store_t initial_topology_block_store;
    topology_block_store_t topology_block_store;
    model_metadata_block_store_t model_metadata_block_store;
    model_sharding_block_store_t model_sharding_block_store;

    // All node clients (active and inactive)
    node_client_map_t node_clients;  
    ip_set_t active_ips; 

    // shared store.
    shared_state_store_ref_t store;
} synchronizer_t;

/*
    Called by the cluster client.
*/

synchronizer_t *synchronizer_create(
        const char *const cluster_id,
        shared_state_store_ref_t store
        ) noexcept
{
    synchronizer_t *synchronizer = new (std::nothrow) synchronizer_t{};
    if (!synchronizer) {
        return nullptr;
    }
    strcpy(
            synchronizer->cluster_id,
            cluster_id
            );
    synchronizer->logger = ubiquity_logger_create(true);
    synchronizer->is_terminating = false; 

    // Initialize prng.
    ubiquity_prng_seed(&synchronizer->prng);

    // Initialize blocks and block hashes.
    topology_block_store_initialize(&synchronizer->topology_block_store);
    model_metadata_block_store_initialize(&synchronizer->model_metadata_block_store);
    model_sharding_block_store_initialize(&synchronizer->model_sharding_block_store);

    // Initialize store
    synchronizer->store = store;
    return synchronizer;
}

void synchronizer_delete(
        synchronizer_t *const synchronizer
        ) noexcept
{
    delete synchronizer;
}

// This function compares two ordered lists using a custom comparison function and applies specific treatments to elements.
static inline void synchronizer_match_list_elements(
        synchronizer_t *const synchronizer,
        void *const old_list,
        const size_t old_count,
        void *const new_list,
        const size_t new_count,
        bool (*const compare_element)(void *const, const size_t, void *const, const size_t),
        void (*const update_element)(synchronizer_t *const, void *const, const size_t, void *const, const size_t),
        void (*const remove_element)(synchronizer_t *const, void *, size_t),
        void (*const add_element)(synchronizer_t *const, void *, size_t)
        ) noexcept
{
    size_t old_cursor = 0;
    size_t new_cursor = 0;
    size_t lookahead_offset = 0;

    // Try matching elements of old list with elements of new list. Look ahead when they don't match.
    while (old_cursor < old_count
            && new_cursor < new_count
            && (old_cursor + lookahead_offset < old_count || new_cursor + lookahead_offset < new_count)) {

        if (0 < lookahead_offset) {

            // If an old element ahead matched current new element, one or many old elements are unmatched and must be removed.
            if (old_cursor + lookahead_offset < old_count
                    && compare_element(
                            old_list,
                            old_cursor + lookahead_offset,
                            new_list, new_cursor
                            )) {
                update_element(
                        synchronizer,
                        old_list,
                        old_cursor + lookahead_offset,
                        new_list,
                        new_cursor
                        );
                for (size_t cursor = old_cursor; cursor < old_cursor + lookahead_offset; ++cursor) {
                    remove_element(
                            synchronizer,
                            old_list,
                            cursor
                            );
                }
                old_cursor += lookahead_offset;
                goto next;
            }

            // If a new element ahead matches current old element, one or many new elements are unmatched and must be added.
            if (new_cursor + lookahead_offset < new_count
                    && compare_element(
                            old_list,
                            old_cursor,
                            new_list,
                            new_cursor + lookahead_offset
                            )) {
                update_element(
                        synchronizer,
                        old_list,
                        old_cursor,
                        new_list,
                        new_cursor + lookahead_offset
                        );
                for (size_t cursor = new_cursor; cursor < new_cursor + lookahead_offset; ++cursor) {
                    add_element(
                            synchronizer,
                            new_list,
                            cursor
                            );
                }
                new_cursor += lookahead_offset;
                goto next;
            }
        } else if (compare_element(
                    old_list,
                    old_cursor,
                    new_list,
                    new_cursor
                    )) {
            update_element(
                    synchronizer,
                    old_list,
                    old_cursor,
                    new_list,
                    new_cursor
                    );
            goto next;
        }

        // Nothing matched ; keep looking ahead.
        ++lookahead_offset;
        continue;

        // 2 element matched ; increment cursors.
        next:
            ++old_cursor;
            ++new_cursor;
            lookahead_offset = 0;
    }

    // Remove old elements that weren't matched.
    while (old_cursor < old_count) {
        remove_element(
                synchronizer,
                old_list,
                old_cursor
                );
        ++old_cursor;
    }

    // Add new elements that weren't matched.
    while (new_cursor < new_count) {
        add_element(
                synchronizer,
                new_list,
                new_cursor
                );
        ++new_cursor;
    }
}


/*
 * Model metadata element matching, adding, update and removing.
 */

static bool model_metadata_compare_elements(
        void *const old_list,
        const size_t old_index,
        void *const new_list,
        const size_t new_index
        ) noexcept
{
    model_metadata_element_t *const new_model_metadata = (model_metadata_element_t *)new_list + new_index;
    model_metadata_element_t *const old_model_metadata = (model_metadata_element_t *)old_list + old_index;

    // If model IDs match, elements match.
    return old_model_metadata->model_id == new_model_metadata->model_id;
}

static void model_metadata_update_element(
        synchronizer_t *const synchronizer,
        void *const old_list,
        const size_t old_index,
        void *const new_list,
        const size_t new_index
        ) noexcept
{
    model_metadata_element_t *const new_model_metadata = (model_metadata_element_t *)new_list + new_index;
    model_metadata_element_t *const old_model_metadata = (model_metadata_element_t *)old_list + old_index;
    model_info_ref_t existing_model_info_ref = store_get_model_info(
            synchronizer->store.get(),
            old_model_metadata->model_id
            );
    if (nullptr == existing_model_info_ref) {
        return;
    }
  
    // FIXEME: check if there were actually changes
    model_info_ref_t model_info_ref = model_info_allocate();
    
    model_info_copy(model_info_ref.get(), existing_model_info_ref.get());
    // first time receiving a model_metadata for this model id
    if(model_info_is_empty(existing_model_info_ref.get())){
        model_info_initialize(
            model_info_ref.get(),
            new_model_metadata->model_id
            );
    model_info_set_application_id(
            model_info_ref.get(),
            new_model_metadata->application_id
            );
    model_info_set_feature_type_list(
            model_info_ref.get(),
            &new_model_metadata->feature_type_list,
            new_model_metadata->feature_type_count
            );
    }
    // if model metadata exists already, then only the feedback delays can be mutated in a model metadata.
    model_info_set_feedback_list(
            model_info_ref.get(),
            &new_model_metadata->feedback_id_list,
            &new_model_metadata->feedback_delay_list,
            new_model_metadata->feedback_count
            );
    store_upsert_model_info(
        synchronizer->store.get(),
        old_model_metadata->model_id,
        model_info_ref
    );
}

static void model_metadata_remove_old_element(
        synchronizer_t *const synchronizer,
        void *element_list,
        size_t index
        ) noexcept
{
    const model_metadata_element_t *const old_model_metadata = (model_metadata_element_t *)element_list + index;
    // Even if the model info is not empty, remove it from the map and release it
    store_remove_model_info(
            synchronizer->store.get(),
            old_model_metadata->model_id
            );
}

static void model_metadata_add_new_element(
        synchronizer_t *const synchronizer,
        void *element_list,
        size_t index
        ) noexcept
{
    const model_metadata_element_t *const new_model_metadata = (model_metadata_element_t *)element_list + index;

    model_info_ref_t existing_model_info_ref = store_get_model_info(
            synchronizer->store.get(),
            new_model_metadata->model_id
            );
    model_info_ref_t model_info_ref = model_info_allocate();    
    if(nullptr != existing_model_info_ref.get() && model_info_has_sharding(existing_model_info_ref.get())){
        // FIXME: copy already existing sharding properly
        model_info_ref.get()->compatible_node_set  = existing_model_info_ref.get()->compatible_node_set;
    }

    model_info_initialize(
            model_info_ref.get(),
            new_model_metadata->model_id
            );
    model_info_set_application_id(
            model_info_ref.get(),
            new_model_metadata->application_id
            );
    model_info_set_feature_type_list(
            model_info_ref.get(),
            &new_model_metadata->feature_type_list,
            new_model_metadata->feature_type_count
            );
    model_info_set_feedback_list(
            model_info_ref.get(),
            &new_model_metadata->feedback_id_list,
            &new_model_metadata->feedback_delay_list,
            new_model_metadata->feedback_count
            );
    store_upsert_model_info(
            synchronizer->store.get(),
            new_model_metadata->model_id,
            model_info_ref
            );
}

/*
 * Model sharding element matching, adding, update and removing.
 */

static bool model_sharding_compare_elements(
        void *const old_list,
        const size_t old_index,
        void *const new_list,
        const size_t new_index
        ) noexcept
{
    const model_sharding_element_t *const old_element = (model_sharding_element_t *)old_list + old_index;
    const model_sharding_element_t *const new_element = (model_sharding_element_t *)new_list + new_index;
    return 0 == memcmp(
            old_element,
            new_element,
            sizeof(model_sharding_element_t)
            );
}

static void model_sharding_update_element(
        synchronizer_t *const synchronizer,
        void *const old_list,
        const size_t old_index,
        void *const new_list,
        const size_t new_index
        ) noexcept
{
}

static void model_sharding_remove_old_element(
        synchronizer_t *const synchronizer,
        void *element_list,
        size_t index
        ) noexcept
{
    const model_sharding_element_t *const old_element = (model_sharding_element_t *)element_list + index;
    model_info_ref_t existing_model_info_ref = store_get_model_info(
            synchronizer->store.get(),
            old_element->model_id
            );
    if (nullptr == existing_model_info_ref) {
        return;
    }

    
    // Model_info exists already, make a copy from exisiting_model_info
    model_info_ref_t model_info_ref = model_info_allocate();
    model_info_copy(model_info_ref.get(), existing_model_info_ref.get());
    
    // Remove node from the model's compatible node list.
    model_info_remove_sharding(
            model_info_ref.get(),
            old_element->ip
            );
    // If the model info is empty, remove it from the map; otherwise update map to point to new copy
    if (model_info_is_empty(model_info_ref.get())) {
        store_remove_model_info(synchronizer->store.get(), old_element->model_id);
    }
    else {
        store_upsert_model_info(synchronizer->store.get(), old_element->model_id, model_info_ref);
    }
}

static void model_sharding_add_new_element(
        synchronizer_t *const synchronizer,
        void *element_list,
        size_t index
        ) noexcept
{
    const model_sharding_element_t *const new_element = (model_sharding_element_t *)element_list + index;
    model_info_ref_t existing_model_info_ref = store_get_model_info(
            synchronizer->store.get(),
            new_element->model_id
            );
    model_info_ref_t model_info_ref = model_info_allocate();
    model_info_t * model_info = model_info_ref.get(); 
    if (nullptr == existing_model_info_ref) {
        // Model_info does not exist, initialize a new one 
        model_info_initialize(
                model_info,
                new_element->model_id
                );
    }
    else {       
        // Model_info exists already, make a copy from exisiting_model_info
        model_info_copy(model_info, existing_model_info_ref.get());
    }

    // Add node to the model's compatible node list.
    model_info_add_sharding(
            model_info,
            new_element->ip
            );
                        
    store_upsert_model_info(
            synchronizer->store.get(),
            new_element->model_id,
            model_info_ref
            );     
}

/*
 * Topology element matching, adding, update and removing.
 */

static bool topology_compare_elements(
        void *const old_list,
        const size_t old_index,
        void *const new_list,
        const size_t new_index
        ) noexcept
{
    topology_element_t *const new_element = (topology_element_t *)new_list + new_index;
    topology_element_t *const old_element = (topology_element_t *)old_list + old_index;

    // If IPs match, elements match.
    return old_element->ip == new_element->ip;
}

static void topology_update_element(
        synchronizer_t *const synchronizer,
        void *const old_list,
        const size_t old_index,
        void *const new_list,
        const size_t new_index
        ) noexcept
{
    topology_element_t *const new_element = (topology_element_t *)new_list + new_index;
    topology_element_t *const old_element = (topology_element_t *)old_list + old_index;
    // Set new node weight if different from old one.
    if (old_element->weight != new_element->weight) {
         LOG_DEBUG_1(
            synchronizer->logger,
            "event=topology_update_element ip={}",
            get_ip_string(old_element->ip)
            );
        auto it = synchronizer->node_clients.find(new_element->ip);
        if(it != synchronizer->node_clients.end()){
            node_client_set_weight(it->second.get(), new_element->weight);
        }
    }
}

static void topology_remove_old_element(
        synchronizer_t *const synchronizer,
        void *element_list,
        size_t index
        ) noexcept
{
    const topology_element_t *const old_element = (topology_element_t *)element_list + index;
    LOG_DEBUG_1(
            synchronizer->logger,
            "event=topology_remove_old_element ip={}",
            get_ip_string(old_element->ip)
            );
    // decrement ref count, which will trigger connection termination once ref count = 0     
    synchronizer->active_ips.erase(
            old_element->ip
            );
    synchronizer->node_clients.erase(
            old_element->ip
            );
    store_remove_node_client(synchronizer->store.get(),
            old_element->ip
            );
}

static void topology_add_new_element(
        synchronizer_t *const synchronizer,
        void *element_list,
        size_t index
        ) noexcept
{
    const topology_element_t *const new_element = (topology_element_t *)element_list + index;
    LOG_DEBUG_1(
            synchronizer->logger,
            "event=topology_add_new_element ip={}",
            get_ip_string(new_element->ip)
            );
    // If the IP is already bound to a node client, skip it.
    node_client_ref_t existing_client;
    auto it = synchronizer->node_clients.find(new_element->ip);
    if(it != synchronizer->node_clients.end()){
        existing_client = it->second;
    }
    if (existing_client) {
        node_client_set_weight(existing_client.get(), new_element->weight);
        LOG_DEBUG_1(
            synchronizer->logger,
            "event=topology_add_new_element_skipped ip={}",
            get_ip_string(new_element->ip)
        );
        return;
    }
    LOG_DEBUG_1(
        synchronizer->logger, 
        "event=node_client_create ip={} msg=\"New node client is being created.\"",
        get_ip_string(new_element->ip)
        );

    // Create a new node client to bind to the new IP.
    node_client_ref_t node_client = node_client_create(
            new_element->ip,
            new_element->weight
            );
    synchronizer->node_clients[new_element->ip] = node_client;
}

/*
 * Reception and parsing of a new block.
 */

#define REPLACE_BLOCK(block_type) do {                                                                                 \
    block_hash_t new_block_hash;                                                                                       \
    compute_md5(                                                                                                       \
            &new_block_hash,                                                                                           \
            new_block_content,                                                                                         \
            new_block_size                                                                                             \
            );                                                                                                         \
                                                                                                                       \
    if (0 != memcmp(&new_block_hash, block_hash, sizeof(block_hash_t))) {                                              \
        LOG_ERROR_6(                                                                                                   \
                synchronizer->logger,                                                                                  \
                "event=apply_block_update status=MISMATCHED_BLOCK_HASH block_type={} block_index={} received_block_hash={:x}{:x} expected_block_hash={:x}{:x} msg=\"Received block hash does not match expected block hash as defined in received block header.\"",\
                #block_type,                                                                                           \
                block_index,                                                                                           \
                new_block_hash.chunks[1],                                                                              \
                new_block_hash.chunks[0],                                                                              \
                block_hash->chunks[1],                                                                                 \
                block_hash->chunks[0]                                                                                  \
                );                                                                                                     \
        synchronizer->CONCAT(block_type, _block_store).successful_update = false;                                      \
        return false;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    const uint16_t old_block_size = synchronizer->CONCAT(block_type, _block_store).block_size_list[block_index];       \
    const uint16_t old_element_count = old_block_size / sizeof(CONCAT(block_type, _element_t));                        \
    const uint16_t new_element_count = new_block_size / sizeof(CONCAT(block_type, _element_t));                        \
                                                                                                                       \
    synchronizer_match_list_elements(                                                                                  \
            synchronizer,                                                                                              \
            synchronizer->CONCAT(block_type, _block_store).block_content_list[block_index].element_list,               \
            old_element_count,                                                                                         \
            ((CONCAT(block_type, _block_t) *)new_block_content)->element_list,                                         \
            new_element_count,                                                                                         \
            CONCAT(block_type, _compare_elements),                                                                     \
            CONCAT(block_type, _update_element),                                                                       \
            CONCAT(block_type, _remove_old_element),                                                                   \
            CONCAT(block_type, _add_new_element)                                                                       \
            );                                                                                                         \
                                                                                                                       \
    CONCAT(block_type, _block_store_update_block)(                                                                     \
            &synchronizer->CONCAT(block_type, _block_store),                                                           \
            block_index,                                                                                               \
            new_block_content,                                                                                         \
            new_block_size                                                                                             \
            );                                                                                                         \
                                                                                                                       \
    CONCAT(block_type, _block_store_update_root_hash)(&synchronizer->CONCAT(block_type, _block_store));                \
                                                                                                                       \
    return true;                                                                                                       \
} while(false)                                                                                                         \

bool synchronizer_apply_block_update(
        synchronizer_t *const synchronizer,
        const uint8_t block_type,
        const uint16_t block_index,
        const uint16_t new_block_size,
        const void *const new_block_content,
        const block_hash_t *const block_hash
        ) noexcept
{
    // Replace local block with received block.
    switch (block_type) {

        case BLOCK_TYPE_TOPOLOGY:
            REPLACE_BLOCK(topology);

        case BLOCK_TYPE_MODEL_SHARDING:
            REPLACE_BLOCK(model_sharding);

        case BLOCK_TYPE_MODEL_METADATA:
            REPLACE_BLOCK(model_metadata);

        default:
            return true;
    }
}

/*
    Initial topology definition and usage.
*/

static int compare_node_ips(const void *alpha, const void *beta) {
    return (ntohl(((topology_element_t *)alpha)->ip) - ntohl(((topology_element_t *)beta)->ip));
}

bool synchronizer_store_initial_topology(
        synchronizer_t *const synchronizer,
        cluster_info_t *const cluster_info
        ) noexcept
{
    topology_block_store_initialize(&synchronizer->initial_topology_block_store);

    // Fill the initial topology blocks.
    for (size_t ip_index = 0; ip_index < cluster_info->ip_count; ++ip_index) {
        const uint32_t ip = cluster_info->ip_list[ip_index];
        const size_t block_index =  XXH3_64bits(&ip, sizeof(ip)) % TOPOLOGY_BLOCK_COUNT;

        // Insert the ip and a weight of 1 in the block.
        topology_element_t *const topology_element = synchronizer->initial_topology_block_store.block_content_list[block_index].element_list
            + (synchronizer->initial_topology_block_store.block_size_list[block_index] / sizeof(topology_element_t));
        synchronizer->initial_topology_block_store.block_size_list[block_index] += sizeof(topology_element_t);
        topology_element->ip = ip;
        topology_element->weight = 1;
    }

    // Sort the elements of each blocks by ascending ip.
    for (size_t block_index = 0; block_index < TOPOLOGY_BLOCK_COUNT; ++block_index) {
        qsort(
                synchronizer->initial_topology_block_store.block_content_list[block_index].element_list,
                synchronizer->initial_topology_block_store.block_size_list[block_index] / sizeof(topology_element_t),
                sizeof(topology_element_t),
                compare_node_ips
                );
    }
    return true;
}

void synchronizer_use_initial_topology(
        synchronizer_t *const synchronizer
        ) noexcept
{
    // Load the initial topology block content into the actual topology blocks.
    for (size_t block_index = 0; block_index < TOPOLOGY_BLOCK_COUNT; ++block_index) {
        const uint16_t block_size = synchronizer->initial_topology_block_store.block_size_list[block_index];
        const void *const block_content = synchronizer->initial_topology_block_store.block_content_list + block_index;
        block_hash_t block_hash;
        compute_md5(
                &block_hash,
                block_content,
                block_size
                );
        synchronizer_apply_block_update(
                synchronizer,
                BLOCK_TYPE_TOPOLOGY,
                block_index,
                block_size,
                block_content,
                &block_hash
                );
    }
}

/*
    Block update request.
*/

struct message_block_request_t {
    message_common_t _;
    uint32_t reserved1;
    uint64_t reserved2;
    root_hash_t root_hash;
    block_hash_t block_hash_list[1];
} __attribute__((packed));

struct message_block_update_t {
    message_common_t _;
    uint16_t block_index;
    uint16_t block_size;
    uint64_t reserved;
    block_hash_t block_hash;
    uint8_t block_content[1];
} __attribute__((packed));

union message_sync_t {
    uint8_t buffer[1];
    message_common_t common;
    message_block_request_t block_request;
    message_block_update_t block_update;
} __attribute__((packed));

#define MESSAGE_TYPE_BLOCK_UPDATE_REQUEST 0x16

#define REQUEST_BLOCK_UPDATE_IF_TIME_HAS_COME(                                                                         \
        block_type,                                                                                                    \
        block_type_index                                                                                               \
        )                                                                                                              \
if (ubiquity_time_is_past(synchronizer->CONCAT(block_type, _block_store).next_request_time)) {                         \
    message_sync_t *const message = (message_sync_t *)synchronizer->message_buffer;                                    \
    message->common.version = SERIALIZATION_VERSION;                                                                   \
    message->common.type = MESSAGE_TYPE_BLOCK_UPDATE_REQUEST + block_type_index;                                       \
    message->block_request.root_hash = synchronizer->CONCAT(block_type, _block_store).root_hash;                       \
    const size_t block_hash_list_size = CONCAT(block_type, _block_store_export_block_hash_list)(                       \
            &synchronizer->CONCAT(block_type, _block_store),                                                           \
            &message->block_request.block_hash_list                                                                    \
            );                                                                                                         \
                                                                                                                       \
    const bool successful_request = STATUS_CODE_SUCCESS == send_message_with_rendezvous(                               \
            synchronizer->node_clients,                                                                                 \
            &synchronizer->active_ips,                                                                                  \
            ubiquity_prng_generate(&synchronizer->prng),                                                               \
            STREAM_ID_SYNC,                                                                                            \
            synchronizer->message_buffer,                                                                              \
            offsetof(message_block_request_t, block_hash_list) + block_hash_list_size                                  \
            );                                                                                                         \
                                                                                                                       \
    const bool perfect_update = successful_request                                                                     \
        && synchronizer->CONCAT(block_type, _block_store).successful_update;                                           \
    synchronizer->CONCAT(block_type, _block_store).request_period =                                                    \
        (perfect_update * MAX_BLOCK_REQUEST_PERIOD_SECONDS)                                                            \
        | (synchronizer->CONCAT(block_type, _block_store).request_period >> 1)                                         \
        | MIN_BLOCK_REQUEST_PERIOD_SECONDS;                                                                            \
                                                                                                                       \
    synchronizer->CONCAT(block_type, _block_store).next_request_time = ubiquity_time_get_from_now(                     \
            UBIQUITY_TIME_SECONDS(synchronizer->CONCAT(block_type, _block_store).request_period)                       \
            );                                                                                                         \
}

/*
    Block update reception.
*/

#define MESSAGE_TYPE_MASK_BLOCK_UPDATE 0x80

static const char *const message_type_strings_1[] = {
    "TOPOLOGY_REQUEST",
    "MODEL_METADATA_REQUEST",
    "MODEL_SHARDING_REQUEST",
};

static const char *const message_type_strings_2[] = {
    "TOPOLOGY_UPDATE",
    "MODEL_METADATA_UPDATE",
    "MODEL_SHARDING_UPDATE",
};

static inline const char *message_type_get_string(int id) {
    if (id < 0x80) {
        return message_type_strings_1[id - 0x40];
    } else {
        return message_type_strings_2[id - 0x80];
    }
}

#define EMPTY_BLOCK_UPDATE_MESSAGE_SIZE offsetof(message_block_update_t, block_content)

static inline void synchronizer_parse_block_update_message(
        synchronizer_t *const synchronizer,
        node_client_t *const node_client
        ) noexcept
{

    message_sync_t *const message = (message_sync_t *)(synchronizer->message_buffer);
    if (0 == (message->common.type & MESSAGE_TYPE_MASK_BLOCK_UPDATE)) {
        LOG_WARNING_4(
                synchronizer->logger,
                "event=invalid_sync_message_type node_client={} ip={} type={} size={} msg=\"Node client : Received synchronization message with invalid type.\"",
                (void *)node_client,
                get_ip_string(
                        node_client_get_ip(node_client)
                        ),
                message->common.type,
                synchronizer->message_size
                );
        return;
    }
    LOG_DEBUG_4(
        synchronizer->logger,
        "event=sync_message_received node_client={} ip={} type={} size={}",
        (void *)node_client,
        get_ip_string(
                node_client_get_ip(node_client)
                ),
        message_type_get_string(message->common.type),
        synchronizer->message_size
        );

    if (EMPTY_BLOCK_UPDATE_MESSAGE_SIZE > synchronizer->message_size) {
        LOG_WARNING_4(
                synchronizer->logger,
                "event=incomplete_block_message node_client={} ip={} minimal_size={} actual_size={}",
                (void *)node_client,
                get_ip_string(
                        node_client_get_ip(node_client)
                        ),
                EMPTY_BLOCK_UPDATE_MESSAGE_SIZE,
                synchronizer->message_size
                );
        return;
    }

    const uint8_t block_type = message->common.type - MESSAGE_TYPE_MASK_BLOCK_UPDATE;

    // Assert computed size of the new block matches provided size of new block.
    const size_t block_size = synchronizer->message_size - EMPTY_BLOCK_UPDATE_MESSAGE_SIZE;
    if (message->block_update.block_size != block_size) {
        LOG_WARNING_4(
                synchronizer->logger,
                "event=mismatched_block_size node_client={} ip={} indicated_size={} actual_size={}",
                (void *)node_client,
                get_ip_string(
                        node_client_get_ip(node_client)
                        ),
                0 + message->block_update.block_size,
                block_size
                );
        return;
    }

    synchronizer_apply_block_update(
            synchronizer,
            block_type,
            message->block_update.block_index,
            message->block_update.block_size,
            message->block_update.block_content,
            &message->block_update.block_hash
            );
}

static inline void receive_synchronization_messages(
        synchronizer_t *const synchronizer,
        node_client_t *const node_client
        ) noexcept
{
    do {
        node_client_poll_message(
                node_client,
                STREAM_ID_SYNC,
                synchronizer->message_buffer,
                &synchronizer->message_size
                );
        if (sizeof(message_common_t) >= synchronizer->message_size) {
            // no message has been polled
            continue;
        }
        synchronizer_parse_block_update_message(
                synchronizer,
                node_client
                );
    } while (0 != synchronizer->message_size);
}

/*
    Function periodically called by the cluster client.
*/

typedef struct node_state_s {
    ip4_t                ip;
    node_client_ref_t    node_client;
    connection_status_t  last_status;
    connection_status_t  current_status;
} node_state_t;

size_t synchronizer_invoke(
        synchronizer_t *const synchronizer
        ) noexcept
{
    size_t connected_nc_count = 0;
    std::vector<node_state_t> states;
    states.reserve(synchronizer->node_clients.size());

    for (const auto& entry : synchronizer->node_clients) {
        ip4_t ip = entry.first;
        node_client_ref_t node_client = entry.second;
        states.push_back({
            ip,
            node_client,
            node_client_get_connection_status(node_client.get()),
            node_client_invoke(node_client.get())
        });
    }

    // 2. Act on decisions.
    for (auto& state : states) {
        if (state.current_status == CONNECTION_UP) {
            ++connected_nc_count;

            receive_synchronization_messages(synchronizer, state.node_client.get());

            if (state.last_status != state.current_status) {
                store_upsert_node_client(synchronizer->store.get(), state.ip, state.node_client);
                synchronizer->active_ips.insert(state.ip);
            }
        }
        else if (state.last_status == CONNECTION_UP) {
            store_remove_node_client(synchronizer->store.get(), state.ip);
            synchronizer->active_ips.erase(state.ip);
        }
    }

    // As long as at least one node is connected, it means the topology is still valid and block updates can be requested.
    if (connected_nc_count > 0) {
        REQUEST_BLOCK_UPDATE_IF_TIME_HAS_COME(topology, BLOCK_TYPE_TOPOLOGY);
        REQUEST_BLOCK_UPDATE_IF_TIME_HAS_COME(model_sharding, BLOCK_TYPE_MODEL_SHARDING);
        REQUEST_BLOCK_UPDATE_IF_TIME_HAS_COME(model_metadata, BLOCK_TYPE_MODEL_METADATA);
    }
    return connected_nc_count;
}

