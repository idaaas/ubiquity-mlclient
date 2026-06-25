#include "components/connection.hpp"

#include "ubiquity_test.hpp"

#include "common/common.hpp"

#include "utils/ubiquity_time.h"

#include <atomic>
#include <unistd.h>
#include <stdio.h>
#include <aeronmd.h>
#include <aeronc.h>
#include <memory>
#include <cstring>
#include <linux/limits.h>
#include <aeron_image.h>
#include <sched.h>

extern aeron_t* g_ubiquity_aeron_client;

aeron_async_add_subscription_t *sub_add[4];
std::atomic<aeron_async_add_publication_t *> pub_add_list[4];
aeron_subscription_t *sub[4];
aeron_publication_t *pub[4];
std::atomic<bool> image_available[4];
std::atomic<int64_t> image_correlation_id[4];

#define URI_BUFFER_SIZE (0x1000)
#define IMAGE_CORRELATION_ID_MAX_SIZE (sizeof("-9223372036854775808\n") + 1)
#define CONNECTION_TIMEOUT_PERIOD UBIQUITY_TIME_SECONDS(5)

#define BUFFER_SENT_MESSAGE 0

void on_available_image(
        void *clientd,
        aeron_subscription_t *subscription,
        aeron_image_t *image
        )
{
    const int32_t stream_id = *(int32_t *)clientd;
    aeron_image_constants_t constants; 
    aeron_image_constants(image, &constants);
    image_correlation_id[stream_id].store(constants.correlation_id, std::memory_order_release);
    image_available[stream_id].store(true, std::memory_order_release);
    fprintf(stdout, "on_available_image fired for stream %d, image correlation_id=%" PRId64 "\n",
        stream_id, image_correlation_id[stream_id].load(std::memory_order_acquire));
    
    ubiquity_test_emit("available image",
            "stream id", stream_id
            );
}

static inline void create_publications(
        const int32_t stream_id
        )
{
    static const int uses_response_channel[] = {
        0,
        1, // STREAM_ID_SYNC
        1, // STREAM_ID_PREDICTION
        0  // STREAM_ID_FEEDBACK
    };

    if (uses_response_channel[stream_id]) {
        printf("                    STREAM_ID : `%i`\n", stream_id);
        char buffer[URI_BUFFER_SIZE];
        char *destination = stpcpy(buffer, "aeron:udp?control-mode=response|control=127.0.0.1:2260|response-correlation-id=");
        snprintf(destination, IMAGE_CORRELATION_ID_MAX_SIZE, "%ld", image_correlation_id[stream_id].load(std::memory_order_acquire));
        aeron_async_add_publication_t *pub_add;
        int result = aeron_async_add_publication(
                &pub_add,
                g_ubiquity_aeron_client,
                buffer,
                stream_id
                );
        if (result < 0) {
            fprintf(stderr, "Failed to add publication: %s\n", aeron_errmsg());
        }
        printf("                    uri : `%s`\n", buffer);
        printf("                    result : `%i`\n", result);
        std::atomic_store(&pub_add_list[stream_id], pub_add);
    }
}

static inline bool poll_pub_add(
        const int32_t stream_id
        )
{
    // Get pub_add.
    aeron_async_add_publication_t *pub_add = std::atomic_load(&pub_add_list[stream_id]);
    if (NULL == pub_add) {
        return false;
    }

    // Get pub.
    const int result = aeron_async_add_publication_poll(&pub[stream_id], pub_add);
    if (0 == result) {
        return false;
    }
    std::atomic_store(&pub_add_list[stream_id], NULL);
    if (-1 == result) {
        fprintf(stderr, "Aeron error: %s\n", aeron_errmsg());
        return false;
    }
    return true;
}

void on_unavailable_image(
        void *clientd,
        aeron_subscription_t *subscription,
        aeron_image_t *image
        ) 
{
    const int32_t stream_id = *(int32_t *)clientd;

    ubiquity_test_emit("unavailable image",
            "stream id", stream_id
            );
}

static inline void create_subscriptions(
        int32_t *const stream_id
        )
{
    printf("opening subscription with stream id %i\n", *stream_id);
    aeron_async_add_subscription(
        &sub_add[*stream_id],
        g_ubiquity_aeron_client,
        "aeron:udp?endpoint=0.0.0.0:2259|response-endpoint=0.0.0.0:2260",
        *stream_id,
        on_available_image, stream_id, on_unavailable_image, stream_id
        );
}

static inline struct timespec get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

// wait_for_subscription
static inline void wait_for_subscription(
        const int32_t stream_id
        )
{
    const struct timespec start = get_time();
    while (1 != aeron_async_add_subscription_poll(&sub[stream_id], sub_add[stream_id])) {
        const struct timespec now = get_time();
        const int64_t elapsed_ns = (now.tv_sec - start.tv_sec) * 1000000000LL
                                 + (now.tv_nsec - start.tv_nsec);
        if (elapsed_ns > 5000000000LL) {
            fprintf(stderr, "Error: wait_for_subscription timed out after 5s on stream %d\n", stream_id);
            exit(1);
        }
        const struct timespec delay = { 0, 1000000 }; // 1ms
        nanosleep(&delay, NULL);
    }
    sub_add[stream_id] = NULL;
}

typedef struct message_handler_arguments_s { 
    uint8_t *polled_message;
    size_t *polled_message_length;
} message_handler_arguments_t;

void server_side_message_handler(
        void *const arguments,
        const uint8_t *const message_buffer,
        const uint64_t message_length,
        aeron_header_stct *const header
        )
{
    (void)header;

    // Branchless min.
    const size_t length_to_copy = 8 ^ ((8 ^ message_length) & -(8 > message_length));

    // Copy message and its length.
    const message_handler_arguments_t *const args = (message_handler_arguments_t *)arguments;
    memcpy(args->polled_message,
            message_buffer,
            length_to_copy);
    *args->polled_message_length = message_length;
}

static inline int send_message(
        connection_t *const connection,
        const int32_t stream_id,
        const bool uses_response_channel
        )
{
    const uint64_t sent_message = 0x12345678;
    const size_t sent_message_length = sizeof(uint64_t);
    status_code_t status_code;

    // Send message client side.
    buffer_clear(BUFFER_SENT_MESSAGE);
    buffer_push_u64(BUFFER_SENT_MESSAGE, sent_message);
    status_code = STATUS_CODE_NO_DATA;
    connection_send_message(
                connection,
                stream_id,
                buffer_content(BUFFER_SENT_MESSAGE),
                buffer_size(BUFFER_SENT_MESSAGE),
                &status_code
                );
    ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, status_code);

    // Poll message server side.
    size_t polled_message_length;
    int result;
    uint64_t polled_message;
    do {
        sched_yield();
        message_handler_arguments_t args = { (uint8_t *)&polled_message, &polled_message_length };
        result = aeron_subscription_poll(sub[stream_id], server_side_message_handler, &args, 1);
    }
    while (0 == result);
    ubiquity_test_assert_inequal(0, result);
    ubiquity_test_assert_inequal(-1, result);
    ubiquity_test_assert_equal(sent_message_length, polled_message_length);
    ubiquity_test_assert_equal(sent_message, polled_message);

    // If there's no response channel, that test is done.
    if (!uses_response_channel) {
        return 0;
    }

    // Send message server side.
    result = -3;
    while (true) {
        if (aeron_publication_is_connected(pub[stream_id])) {
            const auto position = aeron_publication_offer(pub[stream_id], (uint8_t *)&sent_message, sent_message_length, NULL, NULL);
            switch (position) {
                case AERON_PUBLICATION_BACK_PRESSURED:
                    result = -2;
                    break;
                case AERON_PUBLICATION_CLOSED:
                    result = -1;
                    break;
                case AERON_PUBLICATION_ADMIN_ACTION:
                    continue;
                default:
                    result = 0;
                    break;
            }
        }
        break;
    }
    ubiquity_test_assert_equal(0, result);

    // Poll message client side.
    polled_message = 0;
    polled_message_length = 0;
    do {
        connection_poll_message(
                connection,
                stream_id,
                (uint8_t *)&polled_message,
                &polled_message_length
                );
    }
    while (0 == polled_message_length);
    ubiquity_test_assert_equal(sent_message_length, polled_message_length);
    ubiquity_test_assert_equal(sent_message, polled_message);

    return 0;
}

int test_connection(void)
{
    // Constants.
    const char *const ip_string = "127.0.0.1";

    // Variables.
    int32_t stream_id_sync = STREAM_ID_SYNC;
    int32_t stream_id_query = STREAM_ID_PREDICTION;
    int32_t stream_id_feedback = STREAM_ID_FEEDBACK;
    bool result;

    // Initialize the connection module.
    ubiquity_test_assert_true(connection_initialize_module());

    // Ask the media driver to create the subscriptions.
    create_subscriptions(&stream_id_sync);
    create_subscriptions(&stream_id_query);
    create_subscriptions(&stream_id_feedback);

    // Wait for the media driver to actually create the subscriptions.
    fprintf(stdout, "Waiting for subscriptions to be created\n");
    wait_for_subscription(STREAM_ID_SYNC);
    wait_for_subscription(STREAM_ID_PREDICTION);
    wait_for_subscription(STREAM_ID_FEEDBACK);


    // Create the connection.
    connection_ref_t connection_ref = connection_create( ip_string);

    // Ready first connection.
    connection_t *connection = connection_ref.get();
    ubiquity_test_assert_false(connection_ensure_ready(connection));
    fprintf(stdout, "Waiting for connection to initiate pub creation\n");
    bool pubs_created = false;
    int loop_count = 0;
    const struct timespec start = get_time();

    while (!connection_ensure_ready(connection)) {
        loop_count++;

        const struct timespec now = get_time();
        const int64_t elapsed_ns = (now.tv_sec - start.tv_sec) * 1000000000LL
                                + (now.tv_nsec - start.tv_nsec);

        if (!pubs_created &&
            image_available[STREAM_ID_SYNC].load(std::memory_order_acquire) &&
            image_available[STREAM_ID_PREDICTION].load(std::memory_order_acquire))
        {
            // Verify images are actually accessible on the subscription
            if (aeron_subscription_image_count(sub[STREAM_ID_SYNC]) > 0 &&
                aeron_subscription_image_count(sub[STREAM_ID_PREDICTION]) > 0)
            {
                create_publications(STREAM_ID_SYNC);
                create_publications(STREAM_ID_PREDICTION);
                pubs_created = true;
            }

        }

        if (pubs_created && poll_pub_add(STREAM_ID_SYNC) && poll_pub_add(STREAM_ID_PREDICTION)) {
            fprintf(stdout, "Response pubs registered\n");
        }

        // Hard timeout with full diagnostics
        if (elapsed_ns > 5000000000LL) {
            fprintf(stderr, "TIMEOUT after %d iterations\n", loop_count);
            fprintf(stderr, "  image_sync=%d\n",  image_available[STREAM_ID_SYNC].load());
            fprintf(stderr, "  image_query=%d\n", image_available[STREAM_ID_PREDICTION].load());
            fprintf(stderr, "  pubs_created=%d\n", pubs_created);
            fprintf(stderr, "  connection_ensure_ready=%d\n", connection_ensure_ready(connection));
            fflush(stderr);
            exit(1);
        }

        sched_yield();
    }
    
    ubiquity_test_assert_true(connection_ensure_ready(connection));
    return 0;
    do {
        sched_yield();
        result = connection_is_ready(connection);
    }
    while (false == result);
    ubiquity_test_assert_true(connection_is_ready(connection));
    ubiquity_test_expect("available image",
        "stream id", stream_id_sync);
    ubiquity_test_expect("available image",
        "stream id", stream_id_query);
    ubiquity_test_expect("available image",
        "stream id", stream_id_feedback);
    ubiquity_test_done_expecting();

    // Send messages.
    ubiquity_test_assert_true(
            !send_message(
                    connection,
                    STREAM_ID_SYNC,
                    true
                    )
            );
    ubiquity_test_assert_true(
            !send_message(
                    connection,
                    STREAM_ID_PREDICTION,
                    true
                    )
            );
    ubiquity_test_assert_true(
            !send_message(
                    connection,
                    STREAM_ID_FEEDBACK,
                    false
                    )
            );

    // Delete the connection.
    connection_ref = nullptr;
    connection = nullptr;
    ubiquity_time_sleep(CONNECTION_TIMEOUT_PERIOD);
    ubiquity_test_expect("unavailable image",
            "stream id", STREAM_ID_SYNC
            );
    ubiquity_test_expect("unavailable image",
            "stream id", STREAM_ID_PREDICTION
            );
    ubiquity_test_expect("unavailable image",
            "stream id", STREAM_ID_FEEDBACK
            );
    ubiquity_test_done_expecting();
    return 0;
}

int main(void)
{
    ubiquity_test_declare("available image", 1,
            "stream id", "u32"
            );
    ubiquity_test_declare("unavailable image", 1,
            "stream id", "u32"
            );

    ubiquity_test_function_t test_list[] = {
        test_connection,
        0,
    };

    return ubiquity_test_all(test_list);
}


