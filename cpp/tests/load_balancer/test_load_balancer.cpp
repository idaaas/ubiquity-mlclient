#include "components/shared_state_store.hpp"
#include "components/load_balancer.hpp"
#include "ubiquity_test.hpp"
#include "common/common.hpp"
#include "common/model_info.hpp"
#include "utils/prng.hpp"
#include "utils/ubiquity_time.h"

#include <openssl/evp.h>
#include <unistd.h>

/*
 * MOCKS
 */

/*
    Node client.
*/
#define MAX_MESSAGES_PER_NODE 2048
#define QUANTITY_OF_MESSAGES_TO_RECEIVE MAX_MESSAGES_PER_NODE
#define POLLER_LINGER_TIME_NS 10000000
#define MAX_MESSAGE_SIZE 8
#define BALANCED_MESSAGE_SENT_COUNT 0x40000
// Allowed relative deviation from perfect balance.
#define BALANCE_ERROR_MARGIN 0.02
// Weighted balance converges more slowly than uniform balance,
// so we give it a slightly wider margin.
#define WEIGHTED_BALANCE_ERROR_MARGIN 0.05


uint64_t messages_to_send[QUANTITY_OF_MESSAGES_TO_RECEIVE];
size_t received_message_table[QUANTITY_OF_MESSAGES_TO_RECEIVE];

typedef struct message_s {
    uint8_t message_content[MAX_MESSAGE_SIZE]; 
    size_t size;
} message_t;

typedef struct node_client_s { 
    ip4_t ip;
    node_weight_t weight;
    message_t messages[MAX_MESSAGES_PER_NODE];
    size_t message_tail[4];
    size_t message_head[4];    
} node_client_t;


void node_client_delete(
        node_client_t *const node_client
        ) noexcept
{
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
    node_client->weight = (uint64_t)((float)0xffffffffull * weight);
    std::shared_ptr<node_client_t> shared_node_client = std::shared_ptr<node_client_t> (node_client, node_client_delete);
    return shared_node_client;
}


typedef uint64_t computed_node_weight_t;

computed_node_weight_t node_client_get_weight(
        const node_client_t *const node_client
        ) noexcept
{   
    return node_client->weight;
}

static inline void node_client_add_message(
        node_client_t *const node_client,
        const int32_t stream_id,
        uint8_t *const message_content,
        const size_t message_length
        )
{
    message_t *const message = node_client->messages + (node_client->message_tail[stream_id]++ & (MAX_MESSAGES_PER_NODE - 1));
    memcpy(message->message_content, message_content, message_length);
    message->size = message_length;
}

void node_client_poll_message(
        node_client_t *const node_client,
        const int32_t stream_id,
        uint8_t *const destination,
        size_t *const message_length
        ) noexcept
{
    if (node_client->message_head[stream_id] != node_client->message_tail[stream_id]) {
        message_t *const message = node_client->messages + (node_client->message_head[stream_id]++ & (MAX_MESSAGES_PER_NODE - 1));
        if(message->size > 0 && message->size <= MAX_MESSAGE_SIZE){
            memcpy(destination,
                   message->message_content,
                   message->size
                   );
            *message_length = message->size;
        }
    }
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

/*
 * TESTS
 */

int test_rendezvous_uniform_balance(void)
{
    const size_t node_count = 4;
    const double weights[node_count] = {1.0, 1.0, 1.0, 1.0};
    const uint64_t sent_message = 0x1234;
    const size_t msg_len = sizeof(sent_message);
    const stream_id_t stream_id = 1;

    ubiquity_prng_t prng;
    ubiquity_prng_seed(&prng);

    // Create the shared state store.
    shared_state_store_ref_t store = shared_state_store_create();
    ubiquity_test_assert_not_null(store);

    // add all node ips to the ip_hash vector.
    ip_set_t compatible_node_set;        
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        node_client_ref_t node_client = node_client_create((ip4_t)node_index, (node_weight_t)weights[node_index]);
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
        compatible_node_set.emplace((ip4_t)node_index);
    }

    // Send messages with random balance keys.
    status_code_t status_code;
    for (size_t iteration = 0; iteration < BALANCED_MESSAGE_SENT_COUNT; ++iteration) {
        status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set,
                ubiquity_prng_generate(&prng),
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);
    }

    // Count the number of events received for each destination.
    int64_t received_message_count_by_node[node_count];
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        received_message_count_by_node[node_index] = ubiquity_test_count("node_client_send_message",
                "ip", (ip4_t)node_index,
                "stream id", stream_id,
                "message", msg_len, &sent_message
                );
        //fprintf(stderr, "node_index `%lu` ; count `%lu`\n", node_index, received_message_count_by_node[node_index]);
    }

    // Assert that the counts are well in the expected deviation from perfect balance.
    ubiquity_test_assert_almost_equal_array(
            BALANCE_ERROR_MARGIN,
            node_count,
            received_message_count_by_node
            );
            
    ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
            );
    
    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;

    // Delete node clients.
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        ubiquity_test_expect("node_client_delete",
                "ip", node_index
                );
    }
    
    ubiquity_test_done_expecting();

    return 0;
}

int test_rendezvous_weighted_balance(void)
{
    const size_t node_count = 4;
    const double weights[node_count] = {0.1, 0.9, 0.5, 0.7};
    const uint64_t sent_message = 0x1234;
    const size_t msg_len = sizeof(sent_message);
    const stream_id_t stream_id = 1;

    ubiquity_prng_t prng;
    ubiquity_prng_seed(&prng);

    // Create the shared state store.
    shared_state_store_ref_t store = shared_state_store_create();
    ubiquity_test_assert_not_null(store);

    int64_t received_message_count_by_node[node_count];
    status_code_t status_code;
    
    // add all node ips to the ip_hash vector.
    ip_set_t compatible_node_set;        
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        node_client_ref_t node_client = node_client_create((ip4_t)node_index, (node_weight_t)weights[node_index]);
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
        compatible_node_set.emplace((ip4_t)node_index);
    }

    // Send messages with random balance keys.
    for (size_t iteration = 0; iteration < BALANCED_MESSAGE_SENT_COUNT; ++iteration) {
        status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set,
                ubiquity_prng_generate(&prng),
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);
    }

    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        received_message_count_by_node[node_index] = ubiquity_test_count("node_client_send_message",
                "ip", (ip4_t)node_index,
                "stream id", stream_id,
                "message", msg_len, &sent_message
                );
    }

    ubiquity_test_assert_almost_equal_inversly_weighted_array(
        WEIGHTED_BALANCE_ERROR_MARGIN,
        node_count,
        received_message_count_by_node,
        weights
        );

    ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
            );

    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;

    // Delete node clients.
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        ubiquity_test_expect("node_client_delete",
         "ip", node_index
         );
    }
    
    ubiquity_test_done_expecting();
 
    return 0;
}

int test_rendezvous_weight_update(void)
{
    const size_t node_count = 4;
    const double weights_before[node_count] = {1.0, 1.0, 1.0, 1.0};
    const double weights_after[node_count] = {0.1, 0.9, 0.5, 0.7};
    const uint64_t sent_message = 0x1234;
    const size_t msg_len = sizeof(sent_message);
    const stream_id_t stream_id = 1;

    ubiquity_prng_t prng;
    ubiquity_prng_seed(&prng);

    // Create the shared state store.
    shared_state_store_ref_t store = shared_state_store_create();
    ubiquity_test_assert_not_null(store);

    // Add all node ips to the ip_hash vector.
    ip_set_t compatible_node_set;
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        node_client_ref_t node_client = node_client_create((ip4_t)node_index, (node_weight_t)weights_before[node_index]);
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
        compatible_node_set.emplace((ip4_t)node_index);
    }

    // Phase 1: send with uniform weights
    for (size_t iteration = 0; iteration < BALANCED_MESSAGE_SENT_COUNT; ++iteration) {
        status_code_t status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set,
                ubiquity_prng_generate(&prng),
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);
    }

    ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
            );
    ubiquity_test_done_expecting();

    // Update weights.
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        node_client_ref_t node_client = node_client_create((ip4_t)node_index, (node_weight_t)weights_after[node_index]);
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
        ubiquity_test_expect("node_client_delete",
                "ip", node_index
                );
    }
    ubiquity_test_done_expecting();

    // Phase 2 : send again and measure under the new weights.
     for (size_t iteration = 0; iteration < BALANCED_MESSAGE_SENT_COUNT; ++iteration) {
        status_code_t status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set,
                ubiquity_prng_generate(&prng),
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);
    }

    // Count the number of events received for each destination.
    int64_t received_message_count_by_node[node_count];
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        received_message_count_by_node[node_index] = ubiquity_test_count("node_client_send_message",
                "ip", (ip4_t)node_index,
                "stream id", stream_id,
                "message", msg_len, &sent_message
                );
    }

    // Assert that the counts are well in the expected deviation from perfect balance.
    ubiquity_test_assert_almost_equal_inversly_weighted_array(
        WEIGHTED_BALANCE_ERROR_MARGIN,
        node_count,
        received_message_count_by_node,
        weights_after
        );

    ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
            );

    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;

    // Delete node clients.
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        ubiquity_test_expect("node_client_delete",
         "ip", node_index
         );
    }

    ubiquity_test_done_expecting();

    return 0;
}

int test_rendezvous_empty_node_set(void)
{
    const uint64_t sent_message = 0x1234;
    const size_t msg_len = sizeof(sent_message);
    const stream_id_t stream_id = 1;

    ubiquity_prng_t prng;
    ubiquity_prng_seed(&prng);

    // Create the shared state store.
    shared_state_store_ref_t store = shared_state_store_create();
    ubiquity_test_assert_not_null(store);

    ip_set_t empty_nodes; // intentionally empty

    status_code_t status_code = shared_send_message_with_rendezvous(
                store.get(),
                &empty_nodes,
                ubiquity_prng_generate(&prng),
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
    ubiquity_test_assert_equal(STATUS_CODE_NO_COMPATIBLE_NODE, status_code);

    // No send event should have been emitted.
    int64_t total_sends = (int64_t)ubiquity_test_count_partial(
            "node_client_send_message", 1,
            "stream id", stream_id);
    ubiquity_test_assert_equal(0, total_sends);
 
    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;

    ubiquity_test_done_expecting();

    return 0;
}

int test_rendezvous_determinism(void)
{
    const size_t node_count = 8;
    const double weights[node_count] = {1.0, 2.0, 0.5, 1.5, 1.0, 0.8, 2.5, 0.3};
    const uint64_t sent_message = 0x1234;
    const size_t msg_len = sizeof(sent_message);
    const stream_id_t stream_id = 1;
    const size_t key_count = 32;

    // Create the shared state store.
    shared_state_store_ref_t store = shared_state_store_create();
    ubiquity_test_assert_not_null(store);

    // Add all node ips to the ip_hash vector.
    ip_set_t compatible_node_set;
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        node_client_ref_t node_client = node_client_create((ip4_t)node_index, (node_weight_t)weights[node_index]);
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
        compatible_node_set.emplace((ip4_t)node_index);
    }

    // Use fixed balance keys generated with the PRNG.
    // For each of several fixed keys, send twice and assert the
    // same node received both messages.
    ubiquity_prng_t prng;
    ubiquity_prng_seed(&prng);

    for (size_t k = 0; k < key_count; ++k) {
        int64_t key = (int64_t)ubiquity_prng_generate(&prng);

        // First call.
        status_code_t status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set,
                key,
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);

        ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
        );

        // Find which node received it.
        ip4_t selected_node = (ip4_t)-1;
        for (size_t i = 0; i < node_count; ++i) {
            if (1 == ubiquity_test_count(
                        "node_client_send_message",
                        "ip", (ip4_t)i,
                        "stream id", stream_id,
                        "message", msg_len, &sent_message)) {
                selected_node = (ip4_t)i;
                break;
            }
        }
        ubiquity_test_assert_inequal((ip4_t)-1, selected_node);

        ubiquity_test_done_expecting();

        // Second call with the same key.
        status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set,
                key,
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);

        ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
        );

        // Must go to the same node.
        int64_t count = (int64_t) ubiquity_test_count("node_client_send_message",
                        "ip",        selected_node,
                        "stream id", stream_id,
                        "message",   msg_len, &sent_message);
        ubiquity_test_assert_equal(1, count);

        ubiquity_test_done_expecting();
    }

    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;

    // Delete node clients.
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        ubiquity_test_expect("node_client_delete",
         "ip", node_index
         );
    }
 
    ubiquity_test_done_expecting();

    return 0;
}

int test_rendezvous_keys_disruption(void)
{
    const uint64_t sent_message = 0x1234;
    const size_t msg_len = sizeof(sent_message);
    const stream_id_t stream_id = 1;
    const size_t key_count = 302;
    const size_t initial_node_count = 3;
    const size_t final_node_count = 4;
    const double weights[final_node_count] = {1.0, 1.0, 1.0, 1.0};

    // Create the shared state store.
    shared_state_store_ref_t store = shared_state_store_create();
    ubiquity_test_assert_not_null(store);

    // Add all node ips to the ip_hash vector.
    ip_set_t compatible_node_set_before;
    ip_set_t compatible_node_set_after;
    node_client_ref_t node_client;
    for (size_t node_index = 0; node_index < initial_node_count; ++node_index) {
        node_client = node_client_create((ip4_t)node_index, (node_weight_t)weights[node_index]);
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
        compatible_node_set_before.emplace((ip4_t)node_index);
        compatible_node_set_after.emplace((ip4_t)node_index);
    }

    ubiquity_test_done_expecting();
    // Save the selected node for each key before adding the new node.
    ip4_t selected_nodes_before[key_count];
    memset(selected_nodes_before, 0xff, sizeof(selected_nodes_before));
    for (size_t k = 0; k < key_count; ++k) {
        status_code_t status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set_before,
                (int64_t)k,
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);
        ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
        );

        for (size_t i = 0; i < initial_node_count; ++i) {
            if (1 == ubiquity_test_count(
                        "node_client_send_message",
                        "ip", (ip4_t)i,
                        "stream id", stream_id,
                        "message", msg_len, &sent_message)) {
                selected_nodes_before[k] = (ip4_t)i;
                break;
            }
        }
        ubiquity_test_assert_inequal((ip4_t)0xff, selected_nodes_before[k]);
        ubiquity_test_done_expecting();
    }

    // Add the new node.
    node_client = node_client_create((ip4_t)initial_node_count, (node_weight_t)weights[initial_node_count]);
    store_upsert_node_client(store.get(), (ip4_t)initial_node_count, node_client);
    compatible_node_set_after.emplace((ip4_t)initial_node_count);

    // Save the selected node for each key after adding the new node.
    ip4_t selected_nodes_after[key_count];
    memset(selected_nodes_after, 0xff, sizeof(selected_nodes_after));
    for (size_t k = 0; k < key_count; ++k) {
        status_code_t status_code = shared_send_message_with_rendezvous(
                store.get(),
                &compatible_node_set_after,
                (int64_t)k,
                stream_id,
                (uint8_t *)&sent_message,
                msg_len
                );
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);
        ubiquity_test_expect_partial("node_client_send_message", 2,
            "stream id", stream_id,
            "message", msg_len, &sent_message
        );
        for (size_t i = 0; i < final_node_count; ++i) {
            if (1 == ubiquity_test_count("node_client_send_message",
                        "ip", (ip4_t)i,
                        "stream id", stream_id,
                        "message", msg_len, &sent_message)) {
                selected_nodes_after[k] = (ip4_t)i;
                break;
            }
        }
        ubiquity_test_assert_inequal((ip4_t)0xff, selected_nodes_after[k]);
        ubiquity_test_done_expecting();
    }

    // Count remapped keys.
    size_t remapped = 0;
    for (size_t k = 0; k < key_count; ++k) {
        //fprintf(stderr, "key[%zu] before=%d after=%d %s\n",k, selected_nodes_before[k], selected_nodes_after[k], selected_nodes_before[k] != selected_nodes_after[k] ? "REMAPPED" : "");
        if (selected_nodes_before[k] != selected_nodes_after[k])
           ++remapped;
    }

    // Expected remaps ≈ 25 % of keys.
    // Allow ±5 % tolerance.
    const double expected_remap_rate = 1.0 / (double)final_node_count;
    const double actual_remap_rate = (double)remapped / (double)key_count;
    const double tolerance = 0.05;
    const bool within_bounds =
            actual_remap_rate >= expected_remap_rate - tolerance &&
            actual_remap_rate <= expected_remap_rate + tolerance;
    ubiquity_test_assert_equal(within_bounds, true);

    node_client = nullptr; // drop local ref to added node

    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;

    // Delete node clients.
    for (size_t node_index = 0; node_index < final_node_count; ++node_index) {
        ubiquity_test_expect("node_client_delete",
         "ip", node_index
         );
    }
    ubiquity_test_done_expecting();
 
    return 0;
}

int main(void) {
    ubiquity_test_declare("node_client_delete", 1,
            "ip", "u32"
            );

    ubiquity_test_declare("node_client_send_message", 3,
            "ip", "u32",
            "stream id", "i32",
            "message", "buffer"
            );        

    ubiquity_test_function_t test_list[] = {
        test_rendezvous_uniform_balance,
        test_rendezvous_weighted_balance,
        test_rendezvous_weight_update,
        test_rendezvous_empty_node_set,
        test_rendezvous_determinism,
        test_rendezvous_keys_disruption,
        0,
    };
    return ubiquity_test_all(test_list);
}
