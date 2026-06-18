#include "components/shared_state_store.hpp"
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

void *polling_thread_function(
        void *argument
        )
{
    uint64_t received_message;
    size_t received_message_length;

    shared_state_store_t *store = (shared_state_store_t *)argument;

    usleep(20000);

    // Poll messages.
    const int32_t stream_id = 1;
    // ubiquity_time_t end_time = ubiquity_time_get_from_now(POLLER_LINGER_TIME_NS);

    size_t iterations = 0;
    uint32_t total_polled_messages = 0;
    do {
        // fprintf(stderr, "polling iteration %zu\n", iterations); // ← where does it stop?
        ++iterations;
        received_message_length = poll_message(
                store,
                stream_id,
                (uint8_t *)&received_message
                );
        // fprintf(stderr, "received_message_length `%lu`\n", received_message_length);
        if (sizeof(uint64_t) == received_message_length && received_message < QUANTITY_OF_MESSAGES_TO_RECEIVE * 2) {
            if (received_message_table[received_message] > 0) {
                fprintf(stderr, "DUPLICATE: message %lu already received %zu times\n", 
                        received_message, received_message_table[received_message]);
            }
            ++received_message_table[received_message];
            total_polled_messages ++;
            continue;
        }
    }
    while (total_polled_messages < QUANTITY_OF_MESSAGES_TO_RECEIVE);
    usleep(10000);
    return NULL;
}

static inline int poll_message()
{
    // Constants.
    const int32_t stream_id = 1;
    const size_t node_count = 40;
    
    // Create the store.
    shared_state_store_ref_t store = shared_state_store_create();
    node_client_ref_t s_node_client_list [node_count];

    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        node_client_ref_t node_client = node_client_create((ip4_t)node_index, (node_weight_t)1.0);
        s_node_client_list[node_index] = node_client;
        store_upsert_node_client(store.get(), (ip4_t)node_index, node_client);
    }

    // Add messages to be polled.
    for (size_t cursor = 0; cursor < QUANTITY_OF_MESSAGES_TO_RECEIVE; ++cursor) {
        messages_to_send[cursor] = cursor;
        node_client_ref_t random_nc = s_node_client_list[cursor % node_count];
        node_client_add_message( // Test method.
                random_nc.get(),
                stream_id,
                (uint8_t *)(messages_to_send + cursor),
                sizeof(uint64_t)
                );
    }

    // Reset count of received messages.
    for (size_t cursor = 0; cursor < QUANTITY_OF_MESSAGES_TO_RECEIVE; ++cursor) {
        received_message_table[cursor] = 0;
    }

    // Spawn polling thread. It will linger and try to poll for POLLER_LINGER_TIME_NS when there's no message left.
    pthread_t polling_thread; 
    pthread_create(
            &polling_thread,
            NULL,
            polling_thread_function,
            store.get()
            );

    // Wait for poller to terminate.
    pthread_join(polling_thread, NULL);
    

    // Check that all messages are received.
    size_t ones = 0;
    size_t zeros = 0;
    size_t duplicates = 0;

    for (size_t cursor = 0; cursor < QUANTITY_OF_MESSAGES_TO_RECEIVE; ++cursor) {
        if (1 == received_message_table[cursor]) {
            ++ones;
        } else if (0 == received_message_table[cursor]) {
            ++zeros;
        } else {
            ++duplicates;
        }
    }

    // Print full diagnostics before asserting
    fprintf(stderr, "ones: %zu, zeros: %zu, duplicates: %zu\n", ones, zeros, duplicates);

    // Now assert
    ubiquity_test_assert_equal(QUANTITY_OF_MESSAGES_TO_RECEIVE, ones);
    ubiquity_test_assert_equal(0, zeros);
    ubiquity_test_assert_equal(0, duplicates);

    // Delete the store.
    shared_state_store_clear(store.get());
    store = nullptr;
    //Delete node clients.
    for (size_t node_index = 0; node_index < node_count; ++node_index) {
        s_node_client_list[node_index] = nullptr;
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
        poll_message,
        0,
    };
    return ubiquity_test_all(test_list);
}
