#include "components/node_client.hpp"
#include "node_client_config.h"

#include "ubiquity_test.hpp"

#include "common/common.hpp"
#include "utils/ubiquity_time.h"
#include "utils/prng.hpp"

#include <arpa/inet.h>
#include <atomic>
/*
    Connection.
*/

#define MESSAGE_BUFFER_LENGTH 0x100
typedef struct connection_s {
    char ip_string[16];
    client_session_id_t client_session_id;
    uint8_t message_content[MESSAGE_BUFFER_LENGTH];
    size_t message_length;
} connection_t;


typedef std::shared_ptr<connection_t> connection_ref_t;

void connection_delete(
        connection_t *const connection
        ) noexcept
{
    ubiquity_test_emit("connection_delete",
            "ip_string", connection->ip_string
            );
    delete connection;
}

connection_ref_t connection_create(
        const char *const ip_string,
        const client_session_id_t client_session_id
        ) noexcept
{
    connection_t *const connection =  new connection_t();
    strcpy(connection->ip_string, ip_string);
    connection->client_session_id = client_session_id;
    connection->message_length = 0;
    ubiquity_test_emit("connection_create",
            "ip_string", ip_string
            );
    return std::shared_ptr<connection_t>(connection, connection_delete);
}

static bool s_connection_is_ready;

bool connection_ensure_ready(
        connection_t *const connection
        ) noexcept
{
    ubiquity_test_emit("connection_ensure_ready",
            "ip_string", connection->ip_string
            );
    return s_connection_is_ready;
}

bool connection_is_ready(
        connection_t *const connection
        ) noexcept
{
    ubiquity_test_emit("connection_is_ready",
            "ip_string", connection->ip_string
            );
    return s_connection_is_ready;
}


void connection_poll_message(
        connection_t *const connection,
        const stream_id_t stream_id,
        uint8_t *const destination_buffer,
        size_t *const message_length
        ) noexcept
{
    *message_length = 0;
    if (s_connection_is_ready) {
        ubiquity_test_emit("connection_poll_message",
                "ip_string", connection->ip_string,
                "stream id", stream_id
                );
        *message_length = connection->message_length;
        memcpy(destination_buffer, connection->message_content, *message_length);
    }
}

void connection_send_message(
        connection_t *const connection,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_length,
        status_code_t *const status_code
        ) noexcept
{
    ubiquity_test_emit("connection_send_message",
            "ip_string", connection->ip_string,
            "stream id", stream_id
            );
    if (s_connection_is_ready) {
        connection->message_length = message_length;
        memcpy(connection->message_content, message_content, message_length);
        *status_code = STATUS_CODE_SUCCESS;
    }
}

/*
 * TESTS
 */

#define TEST_TIMEOUT_PERIOD UBIQUITY_TIME_MILLISECONDS(500)

uint8_t input_buffer[MAXIMUM_TRANSMISSION_UNIT];

int whole_test(void) {

    // Constants.
    const ip4_t ip = 0x01030304;
    const node_weight_t weight = 1.0f; 
    const char *const ip_string = "4.3.3.1";
    const char *source_message = "Test";
    const size_t source_message_length = 1 + strlen(source_message);
    const stream_id_t stream_id = STREAM_ID_SYNC;

    // Variables.
    char destination_buffer[MESSAGE_BUFFER_LENGTH];
    size_t polled_message_length;
    status_code_t status_code;

    // Create a new node client.
    node_client_ref_t node_client_ref = node_client_create(
            ip,
            weight
            );
   node_client_t *node_client = node_client_ref.get();
    // Assert the ip was correctly set..
    ubiquity_test_assert_equal(
            ip,
            node_client_get_ip(node_client)
            );

    // Poll message from a given stream. Should poll nothing before the connection is established.
    polled_message_length = 0;
    node_client_poll_message(
            node_client,
            stream_id,
            (uint8_t *)destination_buffer,
            &polled_message_length
            );
    ubiquity_test_assert_equal(
            0,
            polled_message_length
            );

    // Send message to a given stream. This should send nothing before the connection is established.
    status_code = STATUS_CODE_NO_DATA;
    node_client_send_message(
            node_client,
            stream_id,
            (uint8_t *)source_message,
            source_message_length,
            &status_code
            );
    ubiquity_test_assert_equal(
            status_code,
            STATUS_CODE_NO_DATA
            );

    // Invoke the node client. It should create a connection and return that it is not connected yet.
    s_connection_is_ready = false;
    ubiquity_test_assert_equal(CONNECTION_CREATED, node_client_invoke(node_client));
    ubiquity_test_expect("connection_create",
        "ip_string", ip_string
        );        
    ubiquity_test_done_expecting();
    
    // Invoke the node client while the connection is created and not ready. It should attempt to ready the connection and return that it is not connected.
    s_connection_is_ready = false;
    ubiquity_test_assert_equal(CONNECTION_CREATED, node_client_invoke(node_client));
    ubiquity_test_expect("connection_ensure_ready",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();

    // Invoke the node client while the connection is ready. It should return that it is connected.
    s_connection_is_ready = true;
    ubiquity_test_assert_equal(CONNECTION_UP, node_client_invoke(node_client));
    ubiquity_test_expect("connection_ensure_ready",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();

    // Invoke the node client while the connection is not ready anymore. It should return it is not connected.
    s_connection_is_ready = false;
    ubiquity_test_assert_equal(CONNECTION_DOWN, node_client_invoke(node_client));
    ubiquity_test_expect("connection_is_ready",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();

    // Invoke the node client after DOWN connection has expired (stayed down for too long)
    s_connection_is_ready = false;
    ubiquity_time_sleep(
            UBIQUITY_TIME_MILLISECONDS(CONNECTION_STATUS_GRACE_PERIOD + 5)
            );
    ubiquity_test_assert_equal(CONNECTION_EMPTY, node_client_invoke(node_client));
    ubiquity_test_expect("connection_is_ready",
        "ip_string", ip_string
        );     
    // It should cleanup expired connection        
    ubiquity_test_expect("connection_delete",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();

    // It should create a new connection.
    ubiquity_test_assert_equal(CONNECTION_CREATED, node_client_invoke(node_client));      
    ubiquity_test_expect("connection_create",
        "ip_string", ip_string
        );

    // Invoke the node client after creating a new connection. It should attempt to make it ready.
    s_connection_is_ready = false;
    ubiquity_test_assert_equal(CONNECTION_CREATED, node_client_invoke(node_client));
    ubiquity_test_expect("connection_ensure_ready",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();

    // Invoke the node client while the connection is ready. It should return that it is connected.
    s_connection_is_ready = true;
    ubiquity_test_assert_equal(CONNECTION_UP, node_client_invoke(node_client));
    ubiquity_test_expect("connection_ensure_ready",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();

    // Send message to a given stream. This should send the source message (store it in a buffer for the purpose of this test).
    status_code = STATUS_CODE_NO_DATA;
    node_client_send_message(
            node_client,
            stream_id,
            (uint8_t *)source_message,
            source_message_length,
            &status_code
            );
    ubiquity_test_assert_equal(
            status_code,
            STATUS_CODE_SUCCESS
            );
    ubiquity_test_expect("connection_send_message",
        "ip_string", ip_string,
        "stream id", stream_id
        );
    ubiquity_test_done_expecting();

    // Poll message from a given stream. Should poll the source message (from the same buffer used above for the purpose of this test).
    polled_message_length = 0;
    node_client_poll_message(
            node_client,
            stream_id,
            (uint8_t *)destination_buffer,
            &polled_message_length
            );
    ubiquity_test_assert_equal(
            source_message_length,
            polled_message_length
            );
    ubiquity_test_assert_equal_string(
            source_message,
            destination_buffer
            );
    ubiquity_test_expect("connection_poll_message",
        "ip_string", ip_string,
        "stream id", stream_id
        );
    ubiquity_test_done_expecting();

    // Deleting the node client should trigger connection termination
    node_client_ref = nullptr;;
    ubiquity_test_expect("connection_delete",
        "ip_string", ip_string
        );
    ubiquity_test_done_expecting();
    return 0;
}

int main(void) {

    ubiquity_test_declare("connection_create", 1,
            "ip_string", "string"
            );
    ubiquity_test_declare("connection_delete", 1,
            "ip_string", "string"
            );
    ubiquity_test_declare("connection_ensure_ready", 1,
            "ip_string", "string"
            );
    ubiquity_test_declare("connection_is_ready", 1,
            "ip_string", "string"
            );            
    ubiquity_test_declare("connection_poll_message", 2,
            "ip_string", "string",
            "stream id", "i16"
            );
    ubiquity_test_declare("connection_send_message", 2,
            "ip_string", "string",
            "stream id", "i16"
            );

    ubiquity_test_function_t test_list[] = {
        whole_test,
        0,
    };
    return ubiquity_test_all(test_list);
}


