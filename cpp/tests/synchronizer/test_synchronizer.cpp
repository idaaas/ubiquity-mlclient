#include "components/synchronizer.hpp"

#include "ubiquity_test.hpp"

#include "common/common.hpp"
#include "common/model_info.hpp"
#include "utils/prng.hpp"
#include "utils/ubiquity_time.h"

#include <openssl/evp.h>
#include <unistd.h>
#include <map>
#include <memory>
/*
 * MOCKS
 */

/*
    Node client.
*/
#define CONNECTION_EMPTY 0
#define CONNECTION_CREATED 1
#define CONNECTION_UP 2
#define CONNECTION_DOWN 3

#define MAX_MESSAGES_PER_NODE 2048
#define QUANTITY_OF_MESSAGES_TO_RECEIVE MAX_MESSAGES_PER_NODE
#define POLLER_LINGER_TIME_NS 10000000
#define BALANCED_MESSAGE_SENT_COUNT 0x40000
#define MAX_MESSAGE_SIZE 8 

typedef uint8_t connection_status_t;

typedef struct node_client_s { 
    ip4_t ip;
    node_weight_t weight;
    connection_status_t connection_status; 

} node_client_t;

typedef std::shared_ptr<node_client_t>  node_client_ref_t;
typedef uint64_t computed_node_weight_t;
typedef std::map<ip4_t, node_client_ref_t> node_client_map_ref;
typedef std::map<model_id_t, model_info_ref_t> model_info_map_ref;

typedef struct shared_state_store_s {
        node_client_map_ref   node_clients;
        model_info_map_ref   models;
} shared_state_store_t;
typedef struct std::shared_ptr<shared_state_store_s> shared_state_store_ref_t;

static shared_state_store_t *s_store = new shared_state_store_t();
static shared_state_store_ref_t s_store_ref = std::shared_ptr<shared_state_store_t>(s_store);

static std::map<ip4_t, connection_status_t> connection_status_map;

static inline void ip_set_connection_status(
        const ip4_t ip,
        connection_status_t connection_status
        ) noexcept
{
    connection_status_map[ip] = connection_status;

    auto it = s_store->node_clients.find(ip);
    if(it != s_store->node_clients.end()){
        it->second->connection_status = connection_status;
    }
}

void node_client_delete(
        node_client_t *const node_client
        ) noexcept
{
    connection_status_map.erase(node_client->ip);
    ubiquity_test_emit("node_client_delete",
            "ip", node_client->ip
            );
    delete node_client;
}

node_client_ref_t node_client_create(
        const ip4_t ip,
        const node_weight_t weight
        ) noexcept
{
    node_client_t *const node_client = new node_client_t();
    node_client->ip = ip;
    node_client->weight = 1;
    ubiquity_test_emit("node_client_create",
            "ip", ip,
            "weight", weight
            );
    return std::shared_ptr<node_client_t>(node_client, node_client_delete);
}

void node_client_set_weight(
        node_client_t *const node_client,
        const node_weight_t weight
        ) noexcept
{
    ubiquity_test_emit("node_client_set_weight",
                "ip", node_client->ip,
                "weight", weight
                );
    node_client->weight = (uint64_t)((float)0xffffffffull * weight);
}

computed_node_weight_t node_client_get_weight(
        const node_client_t *const node_client
        ) noexcept
{
    return node_client->weight;
}

connection_status_t node_client_invoke(
        node_client_t *const node_client
        ) noexcept
{
    ubiquity_test_emit("node_client_invoke",
            "ip", node_client->ip
            );
    auto it = connection_status_map.find(node_client->ip);
    if(it != connection_status_map.end()){
        node_client->connection_status = it->second;
    }
    return node_client->connection_status;
}

connection_status_t node_client_get_connection_status(
            node_client_t *const node_client
            ) noexcept
{
    return node_client->connection_status;  
}

void node_client_send_message(
        node_client_t *const node_client,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size,
        status_code_t *const status_code
        ) noexcept
{
    ubiquity_test_emit("node_client_send_message",
            "ip", node_client->ip,
            "stream id", stream_id,
            "message", message_size, message_content
            );
    *status_code = STATUS_CODE_SUCCESS;
}

void node_client_poll_message(
        node_client_t *const node_client,
        const int32_t stream_id,
        uint8_t *const destination,
        size_t *const message_length
        ) noexcept
{
}

node_client_ref_t store_get_node_client(
        shared_state_store_t* store,
        ip4_t                 key)
{
        auto it   = s_store->node_clients.find(key);
        if (it == s_store->node_clients.end()) return nullptr;
        return it->second;
}

void store_upsert_node_client(
        shared_state_store_t* store,
        ip4_t                 key,
        node_client_ref_t     client)
{
        ubiquity_test_emit("store_upsert_node_client",
            "ip", key
            );
        s_store->node_clients[key] = client;
}        

void store_remove_node_client(
        shared_state_store_t* store,
        ip4_t                 key){
        ubiquity_test_emit("store_remove_node_client",
            "ip", key
            );                
        s_store->node_clients.erase(key);
}

void store_remove_all_node_clients(
        shared_state_store_t* store){
        s_store->node_clients.clear();
}

model_info_ref_t store_get_model_info(
        shared_state_store_t* store,
        model_id_t            key){
        auto it   = s_store->models.find(key);
        if (it == s_store->models.end()) return nullptr;
        return it->second;
}

void store_upsert_model_info(
        shared_state_store_t* store,
        model_id_t            key,
        model_info_ref_t      info){
        ubiquity_test_emit("store_upsert_model_info",
            "model_id", key
            );                
        s_store->models[key] = info;
}

void store_remove_model_info(
        shared_state_store_t* store,
        model_id_t            key){
        ubiquity_test_emit("store_remove_model_info",
            "model_id", key
            );                       

        s_store->models.erase(key);  
}

status_code_t send_message_with_rendezvous(
        const std::map<ip4_t, node_client_ref_t> node_clients,
        const ip_set_t *const targeted_nodes,
        const int64_t balance_key,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size
        )
{        
        return STATUS_CODE_SUCCESS;
}
/*
 * TESTS
 */

static inline int apply_topology_block(
        synchronizer_t *const synchronizer
        )
{
    block_hash_t block_hash;
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    unsigned int md_len = 0;
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, buffer_content(0), buffer_size(0));
    EVP_DigestFinal_ex(ctx, (unsigned char *) &block_hash, &md_len);
    EVP_MD_CTX_destroy(ctx);         

    return synchronizer_apply_block_update(
            synchronizer,
            BLOCK_TYPE_TOPOLOGY,
            0, // block index
            buffer_size(0),
            buffer_content(0),
            &block_hash);
}

#define BLOCK_CONTENT_BUFFER 0

static inline void init_topology_block(void)
{
    buffer_clear(BLOCK_CONTENT_BUFFER);
}

static inline void add_node_and_weight(
        const ip4_t ip,
        const float weight
        )
{
    buffer_push_u32(BLOCK_CONTENT_BUFFER, ip);
    buffer_push_f32(BLOCK_CONTENT_BUFFER, weight);
}

int topology_update(void)
{
    // Constants.
    const char *cluster_id = "some_id";
    ip4_t ip_1 = 1;
    ip4_t ip_2 = 2;
    ip4_t ip_3 = 3;
    ip4_t ip_4 = 4;

    // Variables.
    size_t connected_count;

    // Create synchronizer.
    synchronizer_t *const synchronizer = synchronizer_create(
            cluster_id,
            s_store_ref
            );

    // Invoke the synchronizer. 0 node clients should be connected.
    connected_count = synchronizer_invoke(synchronizer);
    ubiquity_test_assert_equal(0, connected_count);

    // Add first ip.
    init_topology_block();
    add_node_and_weight(ip_1, 0.1);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));       
    ubiquity_test_expect("node_client_create",
            "ip", ip_1,
            "weight", 0.1
            );            
    ubiquity_test_done_expecting();

    // Invoke the synchronizer. 0 node clients should be connected.
    connected_count = synchronizer_invoke(synchronizer);
            
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_1
            );
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal(0, connected_count);

    // Node for ip 1 becomes connected.
    ip_set_connection_status(
            ip_1,
            CONNECTION_UP
            );

    // Invoke the synchronizer. 1 node clients should be connected for the first time, therefore they should be added to shared store
    connected_count = synchronizer_invoke(synchronizer);
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_1
            );
    ubiquity_test_expect("store_upsert_node_client",
            "ip", ip_1
            );             
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal(1, connected_count);

    // Add second ip.
    add_node_and_weight(ip_2, 0.2);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));      
    ubiquity_test_expect("node_client_create",
            "ip", ip_2,
            "weight", 0.2
            );              
    ubiquity_test_done_expecting();

    // Remove second ip.
    init_topology_block();
    add_node_and_weight(ip_1, 0.1);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));

    synchronizer_invoke(synchronizer);
        
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_1
            );
   ubiquity_test_expect("store_remove_node_client",
            "ip", ip_2
            );            
    ubiquity_test_expect("node_client_delete",
            "ip", ip_2
            );            
    ubiquity_test_done_expecting();

    // Add second ip again.
    add_node_and_weight(ip_2, 0.2);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));    
    ubiquity_test_expect("node_client_create",
            "ip", ip_2,
            "weight", 0.2
            );            
    ubiquity_test_done_expecting();

    // Remove first ip.
    init_topology_block();
    add_node_and_weight(ip_2, 0.2);

    ubiquity_test_assert_true(apply_topology_block(synchronizer));
    synchronizer_invoke(synchronizer);    
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_2
            );
    ubiquity_test_expect("store_remove_node_client",
            "ip", ip_1
            );            
    ubiquity_test_expect("node_client_delete",
            "ip", ip_1
            );
    ubiquity_test_done_expecting();

    // Add third ip.
    add_node_and_weight(ip_3, 0.3);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));
    ubiquity_test_expect("node_client_create",
            "ip", ip_3,
            "weight", 0.3
            );          
    ubiquity_test_done_expecting();

    // Make ip 2 and 3 connected.
    ip_set_connection_status(
            ip_2,
            CONNECTION_UP
            );
    ip_set_connection_status(
            ip_3,
            CONNECTION_UP
            );

    // Invoke the synchronizer. 2 node clients should be connected, and added to shared store
    connected_count = synchronizer_invoke(synchronizer);
    ubiquity_test_assert_equal(2, connected_count);
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_2
            );
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_3
            );
    ubiquity_test_expect("store_upsert_node_client",
            "ip", ip_2
            );
    ubiquity_test_expect("store_upsert_node_client",
            "ip", ip_3
            );                          
    ubiquity_test_done_expecting();

    // Add first ip again.
    init_topology_block();
    add_node_and_weight(ip_1, 0.1);
    add_node_and_weight(ip_2, 0.2);
    add_node_and_weight(ip_3, 0.3);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));  
    ubiquity_test_expect("node_client_create",
            "ip", ip_1,
            "weight", 0.1
            );           
    ubiquity_test_done_expecting();

    // Change all weights.
    init_topology_block();
    add_node_and_weight(ip_1, 0.4);
    add_node_and_weight(ip_2, 0.5);
    add_node_and_weight(ip_3, 0.6);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));
    // first one is not connected, so it is not shared yet
    ubiquity_test_expect("node_client_set_weight",
            "ip", ip_1,
            "weight", 0.4
            );        
    ubiquity_test_expect("node_client_set_weight",
            "ip", ip_2,
            "weight", 0.5
            );           
    ubiquity_test_expect("node_client_set_weight",
            "ip", ip_3,
            "weight", 0.6
            );
    ubiquity_test_done_expecting();

    // Replace with only fourth ip.
    init_topology_block();
    add_node_and_weight(ip_4, 0.9);
    ubiquity_test_assert_true(apply_topology_block(synchronizer));

    synchronizer_invoke(synchronizer);

    ubiquity_test_expect("node_client_invoke",
            "ip", ip_4
            ); 
    ubiquity_test_expect("store_remove_node_client",
            "ip", ip_1
            );                       
    ubiquity_test_expect("node_client_delete",
            "ip", ip_1
            );
    ubiquity_test_expect("store_remove_node_client",
            "ip", ip_2
            );            
    ubiquity_test_expect("node_client_delete",
            "ip", ip_2
            );
    ubiquity_test_expect("store_remove_node_client",
            "ip", ip_3
            );            
    ubiquity_test_expect("node_client_delete",
            "ip", ip_3
            );
    // 4th should be created and added locally           
    ubiquity_test_expect("node_client_create",
            "ip", ip_4,
            "weight", 0.9
            );
    // it should be added to shared store once it gets connected
    ip_set_connection_status(
            ip_4,
            CONNECTION_UP
            );
    connected_count = synchronizer_invoke(synchronizer);
    ubiquity_test_expect("node_client_invoke",
            "ip", ip_4
            );
    ubiquity_test_expect("store_upsert_node_client",
            "ip", ip_4
            ); 
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal(1, connected_count);

   // cleanup store
    s_store->node_clients.clear();
    // Delete synchronizer.
    synchronizer_delete(synchronizer);

    // this should stop any remaining node clients
    ubiquity_test_expect("node_client_delete",
            "ip", ip_4
            );

    ubiquity_test_done_expecting();
    return 0;
}

static inline int apply_block(
        synchronizer_t *const synchronizer,
        const size_t block_index,
        const uint8_t block_type
        )
{
    block_hash_t block_hash;
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    unsigned int md_len = 0;
    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, buffer_content(BLOCK_CONTENT_BUFFER), buffer_size(BLOCK_CONTENT_BUFFER));
    EVP_DigestFinal_ex(ctx, (unsigned char *) &block_hash, &md_len);
    EVP_MD_CTX_destroy(ctx);

    return synchronizer_apply_block_update(
            synchronizer,
            block_type,
            block_index,
            buffer_size(BLOCK_CONTENT_BUFFER),
            buffer_content(BLOCK_CONTENT_BUFFER),
            &block_hash
            );
}

static inline bool apply_sharding_block(
        synchronizer_t *const synchronizer,
        const size_t block_index,
        size_t arg_count,
        ...
        )
{
    // Clear block buffer.
    buffer_clear(BLOCK_CONTENT_BUFFER);

    // Append block entries.
    va_list arg_list;
    va_start(arg_list, arg_count);
    for (size_t cursor = 0; cursor < arg_count; ++cursor) {
        buffer_push_u32(BLOCK_CONTENT_BUFFER, va_arg(arg_list, ip4_t));
        buffer_push_u64(BLOCK_CONTENT_BUFFER, va_arg(arg_list, uint64_t));
    }
    va_end(arg_list);
    return apply_block(
        synchronizer,
        block_index,
        BLOCK_TYPE_MODEL_SHARDING
        );
}

typedef struct {
    uint16_t feedback_count;
    feedback_id_list_t feedback_id_list;
    feedback_delay_list_t feedback_delay_list;
} feedback_list_t;

static inline feedback_list_t feedbacks(
        size_t arg_count,
        ...
        )
{
    feedback_list_t feedback_list;
    feedback_id_list_reset(&feedback_list.feedback_id_list);
    feedback_delay_list_reset(&feedback_list.feedback_delay_list);
    va_list arg_list;
    va_start(arg_list, arg_count);
    for (size_t cursor = 0; cursor < arg_count; ++cursor) {
        feedback_list.feedback_id_list.elements[cursor] = va_arg(arg_list, feedback_id_t);
        feedback_list.feedback_delay_list.elements[cursor] = va_arg(arg_list, feedback_delay_t);
    }
    feedback_list.feedback_count = arg_count;
    va_end(arg_list);
    return feedback_list;
}

typedef struct {
    uint16_t feature_type_count;
    feature_type_list_t feature_type_list;
} feature_type_list_with_count_t;

static inline feature_type_list_with_count_t feature_types(
        size_t arg_count,
        ...
        )
{
    feature_type_list_with_count_t feature_type_list_with_count;
    feature_type_list_reset(&feature_type_list_with_count.feature_type_list);

    // Fill in feature types.
    va_list arg_list;
    va_start(arg_list, arg_count);
    for (size_t cursor = 0; cursor < arg_count; ++cursor) {
        feature_type_list_set_element(
                &feature_type_list_with_count.feature_type_list,
                cursor,
                va_arg(arg_list, feature_type_t)
                );
    }
    feature_type_list_with_count.feature_type_count = arg_count;
    va_end(arg_list);
    return feature_type_list_with_count;
}

typedef struct model_metadata_s {
    uint64_t model_id;
    uint16_t schema_version;
    uint16_t application_id;
    uint16_t feedback_count;
    uint16_t feature_type_count;
    feedback_id_list_t feedback_id_list;
    feedback_delay_list_t feedback_delay_list;
    feature_type_list_t feature_type_list;
} __attribute__((packed)) model_metadata_t;

static inline bool apply_metadata_block(
        synchronizer_t *const synchronizer,
        const size_t block_index,
        size_t arg_count,
        ...
        )
{
    // Clear block buffer.
    buffer_clear(BLOCK_CONTENT_BUFFER);
    model_metadata_t model_metadata;

    // Append block entries.
    va_list arg_list;
    va_start(arg_list, arg_count);
    for (size_t cursor = 0; cursor < arg_count; ++cursor) {
        model_metadata.model_id = va_arg(arg_list, uint64_t);
        model_metadata.application_id = va_arg(arg_list, uint64_t);
        feedback_list_t feedback_list = va_arg(arg_list, feedback_list_t);
        model_metadata.feedback_count = feedback_list.feedback_count;
        model_metadata.feedback_id_list = feedback_list.feedback_id_list;
        model_metadata.feedback_delay_list = feedback_list.feedback_delay_list;
        feature_type_list_with_count_t feature_type_list_with_count = va_arg(arg_list, feature_type_list_with_count_t);
        model_metadata.feature_type_count = feature_type_list_with_count.feature_type_count;
        model_metadata.feature_type_list = feature_type_list_with_count.feature_type_list;
        buffer_push_mem(0, &model_metadata, sizeof(model_metadata_t));
    }
    va_end(arg_list);
    return apply_block(
        synchronizer,
        block_index,
        BLOCK_TYPE_MODEL_METADATA
        );
}

int sharding_update(void)
{
    // Constants.
    const char *cluster_id = "some_id";
    const size_t block_id_0 = 0;
    const size_t block_id_1 = 1;
    const model_id_t model_id_1 = 1;
    const ip4_t ip_1 = 123;
    const ip4_t ip_2 = 456;

    // Variables.
    model_info_t *model_info;
    model_info_ref_t model_info_ref;

    // Create synchronizer.
    synchronizer_t *const synchronizer = synchronizer_create(
            cluster_id, 
            s_store_ref
            );

    // Get model info for model 1. Should fail until the model is added to a node.
    model_info_ref = store_get_model_info(s_store, model_id_1);
    ubiquity_test_assert_null(model_info_ref);

    // Add model 1 to ip_1 in block 0 and get model info.
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_0, 1,
                    ip_1, model_id_1
                    )
            );

    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );  
    ubiquity_test_done_expecting();

    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);

    // Model info should have no metadata.
    ubiquity_test_assert_false(model_info_has_metadata(model_info));

    // Model 1 should be compatible with node ip_1.
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );

    // Add model 1 to ip_2 in block 1.
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_1, 1,
                    ip_2, model_id_1
                    )
            );
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );  
   ubiquity_test_done_expecting();

    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_2
                    )
            );

    // Remove model 1 from ip_1 in block 0.
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_0, 0
                    )
            );
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();

    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_false(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_2
                    )
            );

    // Remove model 1 from ip_2 in block 1.
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_1, 0
                    )
            );
    ubiquity_test_expect("store_remove_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();

    model_info_ref = store_get_model_info(s_store, model_id_1);
    ubiquity_test_assert_null(model_info_ref);

    // Delete synchronizer.
    s_store->models.clear();
    synchronizer_delete(synchronizer);
    return 0;
}

int metadata_update(void)
{
    // Constants.
    const char *cluster_id = "some_id";
    const size_t block_id_0 = 0;
    const model_id_t model_id_1 = 1;
    const model_id_t model_id_2 = 2;
    const application_id_t application_id_1 = 2;
    const application_id_t application_id_2 = 3;
    const feedback_id_t feedback_id_1 = 111;
    const feedback_id_t feedback_id_2 = 222;
    const feedback_delay_t feedback_delay_1 = 3;
    const feedback_delay_t feedback_delay_2 = 6;
    feature_type_list_t feature_type_list_1;
    feature_type_list_reset(&feature_type_list_1);
    feature_type_list_set_element(
            &feature_type_list_1,
            0,
            FEATURE_TYPE_CATEGORICAL
            );
    feature_type_list_set_element(
            &feature_type_list_1,
            1,
            FEATURE_TYPE_NUMERIC
            );
    feature_type_list_set_element(
            &feature_type_list_1,
            2,
            FEATURE_TYPE_NUMERIC
            );
    feature_type_list_set_element(
            &feature_type_list_1,
            3,
            FEATURE_TYPE_NUMERIC
            );
    feature_type_list_set_element(
            &feature_type_list_1,
            4,
            FEATURE_TYPE_NUMERIC
            );
    feature_type_list_t feature_type_list_2;
    feature_type_list_reset(&feature_type_list_2);
    feature_type_list_set_element(
            &feature_type_list_2,
            0,
            FEATURE_TYPE_CATEGORICAL
            );
    feature_type_list_set_element(
            &feature_type_list_2,
            1,
            FEATURE_TYPE_NUMERIC
            );

    // Variables.
    model_info_t *model_info;
    model_info_ref_t model_info_ref;

    // Create synchronizer.
    synchronizer_t *const synchronizer = synchronizer_create(
            cluster_id,
            s_store_ref
            );

    // Should fail before the metadata is added.
    model_info_ref = store_get_model_info(s_store, model_id_1);
    ubiquity_test_assert_null(model_info_ref);

    // Add metadata for model 1.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 1,
                    model_id_1, application_id_1,
                        feedbacks(1,
                                feedback_id_1, feedback_delay_1
                        ),
                        feature_types(5,
                                FEATURE_TYPE_CATEGORICAL,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC
                        )
                    )
            );

    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));

    ubiquity_test_assert_equal(model_info->model_id, model_id_1);
    ubiquity_test_assert_equal(model_info->application_id, application_id_1);
    ubiquity_test_assert_equal(model_info->feedback_count, 1);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[0], feedback_id_1);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[0], feedback_delay_1);
    ubiquity_test_assert_equal(model_info->feature_type_count, 5);
    ubiquity_test_assert_true(
            model_info_feature_type_lists_are_identical(
                    model_info,
                    &feature_type_list_1
                    )
            );

    // Modify feeedback list for model 1. Attempt to modify everything else. Only the feedback list should change.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 1,
                    model_id_1, 999,
                        feedbacks(2,
                                feedback_id_1, feedback_delay_1,
                                feedback_id_2, feedback_delay_2
                        ),
                        feature_types(2,
                                FEATURE_TYPE_CATEGORICAL,
                                FEATURE_TYPE_NUMERIC
                        )
                    )
            );

    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));

    // Same attributes as before except for the feedback list.
    ubiquity_test_assert_equal(model_info->model_id, model_id_1);
    ubiquity_test_assert_equal(model_info->application_id, application_id_1);
    ubiquity_test_assert_equal(model_info->feedback_count, 2);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[0], feedback_id_1);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[1], feedback_id_2);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[0], feedback_delay_1);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[1], feedback_delay_2);
    ubiquity_test_assert_equal(model_info->feature_type_count, 5);
    ubiquity_test_assert_true(
            model_info_feature_type_lists_are_identical(
                    model_info,
                    &feature_type_list_1
                    )
            );

    // Remove metadata for model 1.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 0
                    )
            );

    ubiquity_test_expect("store_remove_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    ubiquity_test_assert_null(model_info_ref);

    // Add metadata for model 2.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 1,
                    model_id_2, application_id_2, feedbacks(2,
                                feedback_id_1, feedback_delay_1,
                                feedback_id_2, feedback_delay_2
                            ), feature_types(2,
                                    FEATURE_TYPE_CATEGORICAL,
                                    FEATURE_TYPE_NUMERIC
                                    )
                    )
            );

    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_2
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_2);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));

    ubiquity_test_assert_equal(model_info->model_id, model_id_2);
    ubiquity_test_assert_equal(model_info->application_id, application_id_2);
    ubiquity_test_assert_equal(model_info->feedback_count, 2);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[0], feedback_id_1);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[1], feedback_id_2);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[0], feedback_delay_1);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[1], feedback_delay_2);
    ubiquity_test_assert_equal(model_info->feature_type_count, 2);
    ubiquity_test_assert_true(
            model_info_feature_type_lists_are_identical(
                    model_info,
                    &feature_type_list_2
                    )
            );


    // Add metadata for model 1 while keeping keeping model 2 in the block.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 2,
                    model_id_1, application_id_1,
                        feedbacks(1,
                                feedback_id_1, feedback_delay_1
                        ),
                        feature_types(5,
                                FEATURE_TYPE_CATEGORICAL,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC
                        ),
                    model_id_2, application_id_2,
                        feedbacks(2,
                                feedback_id_1, feedback_delay_1,
                                feedback_id_2, feedback_delay_2
                        ),
                        feature_types(2,
                                FEATURE_TYPE_CATEGORICAL,
                                FEATURE_TYPE_NUMERIC
                        )
                    )
            );

    // Check model 1 was added
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));

    ubiquity_test_assert_equal(model_info->model_id, model_id_1);
    ubiquity_test_assert_equal(model_info->application_id, application_id_1);
    ubiquity_test_assert_equal(model_info->feedback_count, 1);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[0], feedback_id_1);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[0], feedback_delay_1);
    ubiquity_test_assert_equal(model_info->feature_type_count, 5);
    ubiquity_test_assert_true(
            model_info_feature_type_lists_are_identical(
                    model_info,
                    &feature_type_list_1
                    )
            );

    // Check model 2 is still there and valid (updated).
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_2
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_2);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));

    ubiquity_test_assert_equal(model_info->model_id, model_id_2);
    ubiquity_test_assert_equal(model_info->application_id, application_id_2);
    ubiquity_test_assert_equal(model_info->feedback_count, 2);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[0], feedback_id_1);
    ubiquity_test_assert_equal(model_info->feedback_id_list.elements[1], feedback_id_2);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[0], feedback_delay_1);
    ubiquity_test_assert_equal(model_info->feedback_delay_list.elements[1], feedback_delay_2);
    ubiquity_test_assert_equal(model_info->feature_type_count, 2);
    ubiquity_test_assert_true(
            model_info_feature_type_lists_are_identical(
                    model_info,
                    &feature_type_list_2
                    )
            );

    // Remove metadata for models 1 and 2.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 0
                    )
            );

    ubiquity_test_expect("store_remove_model_info",
            "model_id", model_id_1
            );          
    ubiquity_test_expect("store_remove_model_info",
            "model_id", model_id_2
            );                        
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    ubiquity_test_assert_null(model_info_ref);

    model_info_ref = store_get_model_info(s_store, model_id_2);
    ubiquity_test_assert_null(model_info_ref);

    // Delete synchronizer.
    s_store->models.clear();
    synchronizer_delete(synchronizer);
    return 0;
}

int metadata_sharding_update(void)
{
    // Constants.
    const char *cluster_id = "some_id";
    size_t block_id_0 = 0;
    const model_id_t model_id_1 = 1;
    const application_id_t application_id_1 = 2;
    const feedback_id_t feedback_id_1 = 111;
    const feedback_delay_t feedback_delay_1 = 3;
    const ip4_t ip_1 = 123;

    // Variables.
    model_info_t *model_info;
    model_info_ref_t model_info_ref;

    // Create synchronizer.
    synchronizer_t *const synchronizer = synchronizer_create(
            cluster_id,
            s_store_ref
            );

    // Add model 1 to ip_1 in block 0.
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_0, 1,
                    ip_1, 1
                    )
            );
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );          
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_false(model_info_has_metadata(model_info));

    // Model 1 should be compatible with node ip_1.
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );

    // Add metadata for model 1.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 1,
                    model_id_1, application_id_1,
                        feedbacks(1,
                                feedback_id_1, feedback_delay_1
                        ),
                        feature_types(5,
                                FEATURE_TYPE_CATEGORICAL,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC
                        )
                    )
            );

    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));

    // Remove model 1 from ip_1.
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_0, 0
                    )
            );

    // Get model info for model 1. It should still exist because it still has metadata.
        ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );            
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);

    // Model 1 should not be compatible with node ip_1 anymore.
    ubiquity_test_assert_false(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );

    // Remove metadata for model 1.
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 0
                    )
            );

    // Get model info for model 1. It should not exist anymore because there's no metadata nor sharding for this model.
    ubiquity_test_expect("store_remove_model_info",
            "model_id", model_id_1
            );          
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_null(model_info);

    // Add sharding and metadata again...
    // first sharding
    ubiquity_test_assert_true(
            apply_sharding_block(synchronizer, block_id_0, 1,
                    ip_1, 1
                    )
            );
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );          
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_false(model_info_has_metadata(model_info));
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );
    // now metadata 
    ubiquity_test_assert_true(
            apply_metadata_block(synchronizer, block_id_0, 1,
                    model_id_1, application_id_1,
                        feedbacks(1,
                                feedback_id_1, feedback_delay_1
                        ),
                        feature_types(5,
                                FEATURE_TYPE_CATEGORICAL,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC,
                                FEATURE_TYPE_NUMERIC
                        )
                    )
            );
    ubiquity_test_expect("store_upsert_model_info",
            "model_id", model_id_1
            );          
    ubiquity_test_done_expecting();
    model_info_ref = store_get_model_info(s_store, model_id_1);
    model_info = model_info_ref == nullptr ? NULL : model_info_ref.get();
    ubiquity_test_assert_not_null(model_info);
    ubiquity_test_assert_true(model_info_has_metadata(model_info));
    ubiquity_test_assert_true(
            model_info_has_sharding_for_ip(
                    model_info,
                    ip_1
                    )
            );

    // Delete synchronizer.
    s_store->models.clear();
    synchronizer_delete(synchronizer);
    return 0;
}

int main(void) {
    ubiquity_test_declare("node_client_create", 2,
            "ip", "u32",
            "weight", "f32"
            );
    ubiquity_test_declare("node_client_set_weight", 1,
            "ip", "u32",
            "weight", "f32"
            );            

    ubiquity_test_declare("node_client_delete", 1,
            "ip", "u32"
            );

    ubiquity_test_declare("node_client_invoke", 1,
            "ip", "u32"
            );

    ubiquity_test_declare("node_client_send_message", 3,
            "ip", "u32",
            "stream id", "i32",
            "message", "buffer"
            ); 
    ubiquity_test_declare("node_client_poll_message", 2,
            "ip", "u32",
            "stream id", "i32"
            );                                 
    ubiquity_test_declare("store_upsert_node_client", 1,
            "ip", "u32"
            );   
    ubiquity_test_declare("store_remove_node_client", 1,
            "ip", "u32"
            ); 
    ubiquity_test_declare("store_upsert_model_info", 1,
            "model_id", "u64"
            );   
    ubiquity_test_declare("store_remove_model_info", 1,
            "model_id", "u64"
            );                                

    ubiquity_test_function_t test_list[] = {
        topology_update,
        sharding_update,
        metadata_update,
        metadata_sharding_update,
        0,
    };
    return ubiquity_test_all(test_list);
}


