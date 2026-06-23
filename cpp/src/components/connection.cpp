#include "config/config.hpp"
#include "common/common.hpp"
#include "utils/logger.hpp"
#include "connection.hpp"
#include <aeronmd.h>
#include <aeronc.h>
#include <memory>
#include <cstring>
#include <linux/limits.h>
#include <arpa/inet.h>

#define MAXIMUM_STREAM_ID 4

typedef struct fragment_handler_arguments_s { 
    uint8_t *message_buffer;
    size_t *message_length;
} fragment_handler_arguments_t;

typedef struct connection_s {
    char ip_string[INET_ADDRSTRLEN];
    client_session_id_t client_session_id;

    aeron_async_add_publication_t *pub_add[MAXIMUM_STREAM_ID];
    aeron_async_add_subscription_t *sub_add[MAXIMUM_STREAM_ID];
    aeron_publication_t *pub[MAXIMUM_STREAM_ID];
    aeron_subscription_t *sub[MAXIMUM_STREAM_ID];
    aeron_fragment_assembler_t *fragment_assembler[MAXIMUM_STREAM_ID];
    fragment_handler_arguments_t handler_arguments[MAXIMUM_STREAM_ID];
    int64_t response_correlation_id[MAXIMUM_STREAM_ID];
} connection_t;

static aeron_driver_t *s_aeron_driver = NULL;
static aeron_driver_context_t *s_aeron_driver_context = NULL;
aeron_t* g_ubiquity_aeron_client;
static aeron_context_t* s_aeron_client_context;
static char s_directory_path[PATH_MAX] = DEFAULT_LOG_BUFFER_DIRECTORY;

static ubiquity_logger_t s_logger;

bool connection_set_directory(
        const char *const directory_path
        ) noexcept
{
    const size_t path_length = strlen(directory_path);
    if (PATH_MAX < path_length) {
        LOG_ERROR_2(s_logger, "event=connection_set_directory status=PATH_TOO_LONG path={} path_kept={} msg=\"Length of provided aeron directory path exceeds system maximum path length. Keeping current path.\"",
                directory_path,
                s_directory_path
                );
        return false;
    }
    memcpy(s_directory_path,
            directory_path,
            path_length + 1);
    return true;
}

static void aeron_client_error_handler(
        void *clientd,
        int error_code,
        const char *message
        ) {
    (void)clientd;
    LOG_ERROR_2(s_logger, "event=aeron_error error_code={} error_message=\"{}\"",
            error_code,
            message
            );
}

static bool s_initialized = false;

bool connection_initialize_module(
        ) noexcept
{
    if (s_initialized) {
        return STATUS_CODE_SUCCESS;
    }
    s_initialized = true;
    s_logger = ubiquity_logger_create(true);
    LOG_TRACE_0(s_logger, "event=connector_initialize msg=\"Connector : Starting initialization.\"");

    // Initialize Aeron driver context.
    if (aeron_driver_context_init(&s_aeron_driver_context) < 0) {
        LOG_ERROR_0(s_logger, "event=connector_initialization status=AERON_DRIVER_CONTEXT_INIT_ERROR msg=\"Aeron driver context initialization failed.\"");
        goto initialization_failure_1;
    }
    aeron_driver_context_set_agent_on_start_function(
            s_aeron_driver_context,
            aeron_set_thread_affinity_on_start,
            s_aeron_driver_context
            );
    aeron_driver_context_set_dir(
            s_aeron_driver_context,
            s_directory_path
            );
    aeron_driver_context_set_dir_delete_on_shutdown(
            s_aeron_driver_context,
            true
            );
    aeron_driver_context_set_dir_delete_on_start(
            s_aeron_driver_context,
            true
            );
    aeron_driver_context_set_mtu_length(
            s_aeron_driver_context,
            MTU_LENGTH
            );
    aeron_driver_context_set_perform_storage_checks(
            s_aeron_driver_context,
            true
            );
    aeron_driver_context_set_receiver_idle_strategy(
            s_aeron_driver_context,
            "backoff"
            );
    aeron_driver_context_set_rejoin_stream(
            s_aeron_driver_context,
            true
            );
    aeron_driver_context_set_reliable_stream(
            s_aeron_driver_context,
            true
            );
    aeron_driver_context_set_sender_idle_strategy(
            s_aeron_driver_context,
            "backoff"
            );
    aeron_driver_context_set_shared_idle_strategy(
            s_aeron_driver_context,
            "backoff"
            );
    aeron_driver_context_set_threading_mode(
            s_aeron_driver_context,
            AERON_THREADING_MODE_SHARED
            );
    aeron_driver_context_set_term_buffer_sparse_file(
            s_aeron_driver_context,
            false
            );
    aeron_driver_context_set_term_buffer_length(
            s_aeron_driver_context,
            TERM_BUFFER_LENGTH
            );

#ifndef NDEBUG
    aeron_driver_context_set_print_configuration(s_aeron_driver_context, true);
#endif

    // Initialize Aeron driver.
    if(0 > aeron_driver_init(
                &s_aeron_driver,
                s_aeron_driver_context
                )) {
        LOG_ERROR_0(s_logger, "event=connector_initialization status=AERON_DRIVER_INIT_ERROR msg=\"Aeron driver initialization failed.\"");
        goto initialization_failure_2;
    }

    // Start aeron driver.
    if(0 > aeron_driver_start(
                s_aeron_driver,
                false
                )) {
        LOG_ERROR_0(s_logger, "event=connector_initialization status=AERON_DRIVER_START_ERROR msg=\"Aeron driver start failed.\"");
        goto initialization_failure_3;
    }

    // Initialize Aeron client context.
    aeron_context_init(&s_aeron_client_context);
    aeron_context_set_error_handler(
            s_aeron_client_context,
            aeron_client_error_handler,
            NULL
            );
    aeron_context_set_dir(
            s_aeron_client_context,
            s_directory_path
            );
    aeron_context_set_pre_touch_mapped_memory(
            s_aeron_client_context,
            true
            );

    // Initialize Aeron client.
    if (0 > aeron_init(
                &g_ubiquity_aeron_client,
                s_aeron_client_context
                )) {
        LOG_ERROR_0(s_logger, "event=connector_initialization status=AERON_INIT_ERROR msg=\"Aeron client initialization failed.\"");
        goto initialization_failure_4;
    }

    // Start Aeron client.
    if (0 > aeron_start(g_ubiquity_aeron_client)) {
        LOG_ERROR_0(s_logger, "event=connector_initialization status=AERON_START_ERROR msg=\"Aeron client start failed.\"");
        goto initialization_failure_5;
    }

    // Successfully initialized Aeron.
    LOG_INFO_0(s_logger, "event=connector_initialization msg=\"Connector : Successful initialization.\"");

    return true;
    initialization_failure_5:
        aeron_close(g_ubiquity_aeron_client);
        g_ubiquity_aeron_client = NULL;
    initialization_failure_4:
        aeron_context_close(s_aeron_client_context);
        s_aeron_client_context = NULL;
    initialization_failure_3:
        aeron_driver_close(s_aeron_driver);
        s_aeron_driver = NULL;
    initialization_failure_2:
        aeron_driver_context_close(s_aeron_driver_context);
        s_aeron_driver_context = NULL;
    initialization_failure_1:
        ubiquity_logger_delete(s_logger);
        return false;
}

void connection_close_module(
        ) noexcept
{
    if (s_initialized) {

        // Close the Aeron client before closing the Aeron driver.
        LOG_TRACE_0(s_logger, "event=connector_termination msg=\"Connector : termination started.\"");
        aeron_close(g_ubiquity_aeron_client);
        aeron_context_close(s_aeron_client_context);
        aeron_driver_close(s_aeron_driver);
        aeron_driver_context_close(s_aeron_driver_context);
        g_ubiquity_aeron_client = NULL;
        s_aeron_client_context = NULL;
        s_aeron_driver = NULL;
        s_aeron_driver_context = NULL;
        LOG_INFO_0(s_logger, "event=connector_termination msg=\"Connector : terminated.\"");
        ubiquity_logger_delete(s_logger);
    }
}

static void fragment_handler(
        void *const arguments,
        const uint8_t *const message_buffer,
        const uint64_t message_length,
        aeron_header_stct *const header
        ) noexcept
{
    (void)header;

    // Ensure that no more than the destination buffer size is copied.
    const size_t length_to_copy = message_length > MAXIMUM_MESSAGE_SIZE
        ? MAXIMUM_MESSAGE_SIZE
        : message_length;

    // Copy message and its length.
    const fragment_handler_arguments_t *const args = (fragment_handler_arguments_t *)arguments;
    memcpy(args->message_buffer, message_buffer, length_to_copy);
    *args->message_length = message_length;
}

static bool uses_extended_term_buffer[MAXIMUM_STREAM_ID] = {
    false,  // NONE
    true,   // STREAM_ID_SYNC
    false,  // STREAM_ID_PREDICTION
    false   // STREAM_ID_FEEDBACK
};

void connection_init(
        connection_t *const connection,
        const char *const ip_string,
        const client_session_id_t client_session_id
        ) noexcept
{
    // Store the ip string.
    strcpy(connection->ip_string, ip_string);

    // Client session id is used cluster side to identify a client session through connection session id.
    connection->client_session_id = client_session_id;

    // Each stream using extented term buffer needs a fragment assembler.
    for (size_t stream_id = 0; stream_id < MAXIMUM_STREAM_ID; ++stream_id) {
        if (uses_extended_term_buffer[stream_id]) {
            aeron_fragment_assembler_create(
                    &connection->fragment_assembler[stream_id],
                    fragment_handler,
                    &connection->handler_arguments[stream_id]
                    );
        }
        else {
            connection->fragment_assembler[stream_id] = nullptr; // explicit intent
        }
    }
    LOG_DEBUG_2(s_logger, "event=connection_create client_session_id={} ip={}",
        client_session_id,
        ip_string
        );
}

connection_ref_t connection_create(
        const char *const ip_string,
        const client_session_id_t client_session_id
        ) noexcept
{
    if (INET_ADDRSTRLEN <= strlen(ip_string)) {
        return nullptr;
    }

    connection_t *connection = new (std::nothrow) connection_t{};
    if(!connection){
        return nullptr;
    }

    connection_init(connection, ip_string, client_session_id);

    return std::shared_ptr<connection_t>(connection, connection_delete);
}


#define SESSION_ID_MAX_SIZE (sizeof("4294967295") + 1)
#define RESPONSE_CORRELATION_ID_MAX_SIZE (sizeof("-9223372036854775808") + 1)
#define URI_BUFFER_SIZE (0x1000)

static inline char *get_request_channel_uri(
        const char *const endpoint_string,
        const uint32_t session_id,
        const int64_t response_correlation_id,
        const bool extend_term_buffer_length
        )
{
    static char buffer[URI_BUFFER_SIZE];

    char *destination = buffer;
    destination = stpcpy(destination, "aeron:udp?endpoint=");
    destination = stpcpy(destination, endpoint_string);
    destination = stpcpy(destination, ":2259");
    if (0 != response_correlation_id) {
        destination = stpcpy(destination, "|response-endpoint=");
        destination = stpcpy(destination, endpoint_string);
        destination = stpcpy(destination, ":2260|response-correlation-id=");
        destination += snprintf(destination, RESPONSE_CORRELATION_ID_MAX_SIZE, "%ld", response_correlation_id);
    }
    if (extend_term_buffer_length) {
        destination = stpcpy(destination, "|term-length=256k");
    }
   // destination = stpcpy(destination, "|session-id=");
   // snprintf(destination, SESSION_ID_MAX_SIZE, "%d", session_id);
    return buffer;
}

static inline char *get_response_channel_uri(
        const char *const endpoint_string
        )
{
    static char buffer[URI_BUFFER_SIZE];

    char *destination = buffer;
    destination = stpcpy(destination, "aeron:udp?control-mode=response|control=");
    destination = stpcpy(destination, endpoint_string);
    destination = stpcpy(destination, ":2260");
    return buffer;
}

static inline bool is_stream_ready(
        connection_t *const connection,
        const stream_id_t stream_id,
        const bool has_response
        )
{
    return NULL != connection->pub[stream_id]
        && aeron_publication_is_connected(connection->pub[stream_id])
        && (
            !has_response
            || (
                NULL != connection->sub[stream_id]
                && aeron_subscription_is_connected(connection->sub[stream_id])
                )
            );
}

bool connection_is_ready(
        connection_t *const connection
        ) noexcept {
    return is_stream_ready(connection, STREAM_ID_SYNC, true)
        && is_stream_ready(connection, STREAM_ID_PREDICTION, true)
        && is_stream_ready(connection, STREAM_ID_FEEDBACK, false);
}

static inline void initiate_stream_subscription_creation(
        connection_t *const connection,
        const stream_id_t stream_id
        )
{
    aeron_async_add_subscription(
            &connection->sub_add[stream_id],
            g_ubiquity_aeron_client,
            get_response_channel_uri(connection->ip_string),
            stream_id,
            NULL, NULL, NULL, NULL);
    connection->response_correlation_id[stream_id] = aeron_async_add_subscription_get_registration_id(connection->sub_add[stream_id]);
}

static inline bool conclude_stream_subscription_creation(
        connection_t *const connection,
        const stream_id_t stream_id
        )
{
    aeron_subscription_t* sub;
    const int result = aeron_async_add_subscription_poll(
            &sub,
            connection->sub_add[stream_id]
            );

    // Still creating.
    if (0 == result) {
        return false;
    }

    // Created (result 1) or aborted.
    if (1 == result) {
        connection->sub[stream_id]= sub;
    }
    connection->sub_add[stream_id] = NULL;
    return true;
}

static inline void initiate_stream_publication_creation(
        connection_t *const connection,
        const stream_id_t stream_id,
        const bool has_response
        )
{
    aeron_async_add_publication(
            &connection->pub_add[stream_id],
            g_ubiquity_aeron_client,
            get_request_channel_uri(
                    connection->ip_string,
                    connection->client_session_id ^ stream_id,
                    has_response * connection->response_correlation_id[stream_id],
                    uses_extended_term_buffer[stream_id]),
            stream_id);
}

static inline bool conclude_stream_publication_creation(
        connection_t *const connection,
        const stream_id_t stream_id
        )
{
    aeron_publication_t* pub;
    const int result = aeron_async_add_publication_poll(
            &pub,
            connection->pub_add[stream_id]
            );

    // Still creating.
    if (0 == result) {
        return false;
    }

    // Created (result 1) or aborted.
    if (1 == result) {
        connection->pub[stream_id] = pub;
    }
    connection->pub_add[stream_id] = NULL;
    return true;
}

static inline void ensure_stream_ready(
        connection_t *const connection,
        const stream_id_t stream_id,
        const bool has_response
        )
{
    if (has_response && NULL == connection->sub[stream_id]) {
        if (NULL == connection->sub_add[stream_id]) {
            initiate_stream_subscription_creation(
                    connection,
                    stream_id
                    );
        }
        conclude_stream_subscription_creation(
                connection,
                stream_id
                );
    }
    if (NULL == connection->pub[stream_id]) {
        if (NULL == connection->pub_add[stream_id]) {
            initiate_stream_publication_creation(
                    connection,
                    stream_id,
                    has_response
                    );
        }
        conclude_stream_publication_creation(
                connection,
                stream_id
                );
    }
}

bool connection_ensure_ready(
        connection_t *const connection
        ) noexcept
{
    ensure_stream_ready(connection, STREAM_ID_SYNC, true);
    ensure_stream_ready(connection, STREAM_ID_PREDICTION, true);
    ensure_stream_ready(connection, STREAM_ID_FEEDBACK, false);
    return connection_is_ready(connection);
}

static inline bool close_and_free_stream_publication(
        connection_t *const connection,
        const stream_id_t stream_id
        )
{
    if (NULL != connection->pub_add[stream_id]
        && NULL == connection->pub[stream_id]
        && !conclude_stream_publication_creation(
                connection,
                stream_id
                )) {
        return false;
    }
    if (NULL != connection->pub[stream_id]) {
        aeron_publication_close(connection->pub[stream_id], NULL, NULL);
        connection->pub[stream_id] = NULL;
    }
    return true;
}

static inline bool close_and_free_stream_subscription(
        connection_t *const connection,
        const stream_id_t stream_id
        )
{
    if (NULL != connection->sub_add[stream_id]
            && NULL == connection->sub[stream_id]
            && !conclude_stream_subscription_creation(
                connection,
                stream_id
                )) {
        return false;
    }
    if (NULL != connection->sub[stream_id]) {
        aeron_subscription_close(connection->sub[stream_id], NULL, NULL);
        connection->sub[stream_id] = NULL;
    }
    return true;
}

static inline bool connection_terminate(
        connection_t *const connection
        ) noexcept
{
    // Attempt to close and free all publications and subscriptions.
    bool result = true;
    result = result && close_and_free_stream_subscription(
            connection,
            STREAM_ID_SYNC
            );
    result = result && close_and_free_stream_publication(
            connection,
            STREAM_ID_SYNC
            );
    result = result && close_and_free_stream_subscription(
            connection,
            STREAM_ID_PREDICTION
            );
    result = result && close_and_free_stream_publication(
            connection,
            STREAM_ID_PREDICTION
            );
    result = result && close_and_free_stream_publication(
            connection,
            STREAM_ID_FEEDBACK
            );
    return result;
}

void connection_delete(
        connection_t *const connection
        ) noexcept
{
    LOG_DEBUG_2(s_logger, "event=connection_delete client_session_id={} ip={}",
        connection->client_session_id,
        connection->ip_string
        );
    connection_terminate(connection);
    // Delete the fragment assembler of each stream that are using extented term buffer.
    for (size_t stream_id = 0; stream_id < MAXIMUM_STREAM_ID; ++stream_id) {
        if (uses_extended_term_buffer[stream_id]) {
            aeron_fragment_assembler_delete(connection->fragment_assembler[stream_id]);
        }
    }
    delete connection;
}

#define FRAGMENT_LIMIT 1 // Must always be 1 for now because the assembler arguments share the same buffer.

void connection_poll_message(
        connection_t *const connection,
        const stream_id_t stream_id,
        uint8_t *const destination_buffer,
        size_t *const message_length
        ) noexcept
{
    *message_length = 0;
    aeron_subscription_t *sub = connection->sub[stream_id];

    // If the subscription is not connected, leave.
    if (__glibc_unlikely(NULL == sub || !aeron_subscription_is_connected(sub))) {
        return;
    }

    // Store the arguments so the fragment handler can use them if a message is polled.
    connection->handler_arguments[stream_id].message_buffer = destination_buffer;
    connection->handler_arguments[stream_id].message_length = message_length;

    if (uses_extended_term_buffer[stream_id]) {

        // Poll using the fragment assembler to handle fragments so it passes complete messages to the fragment handler..
        aeron_subscription_poll(
                sub,
                aeron_fragment_assembler_handler,
                connection->fragment_assembler[stream_id],
                FRAGMENT_LIMIT
                );
    } else {

        // Poll using fragment handler directly to handle fragments.
        aeron_subscription_poll(
                sub,
                fragment_handler,
                &connection->handler_arguments[stream_id],
                FRAGMENT_LIMIT
                );
    }
}

void connection_send_message(
        connection_t *const connection,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size,
        status_code_t *const status_code
        ) noexcept
{
    // Try sending the message until Aeron is done doing admin actions or until an error prevents the sending.
    while (true) {
        aeron_publication_t *pub = connection->pub[stream_id];
        if (__glibc_likely(NULL != pub && aeron_publication_is_connected(pub))) {
            const auto position = aeron_publication_offer(
                    pub,
                    message_content,
                    message_size,
                    NULL,
                    NULL
                    );
            switch (position) {
                case AERON_PUBLICATION_BACK_PRESSURED:
                    *status_code = STATUS_CODE_BACK_PRESSURED;
                    break;
                case AERON_PUBLICATION_CLOSED:
                    break;
                case AERON_PUBLICATION_ERROR:
                    break;
                case AERON_PUBLICATION_ADMIN_ACTION:
                    continue;
                default:
                    *status_code = STATUS_CODE_SUCCESS;
                    return;
            }
        }
        break;
    }
}
