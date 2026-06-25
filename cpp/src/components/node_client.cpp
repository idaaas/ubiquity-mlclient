#include "node_client.hpp"
#include "node_client_config.h"

#include "connection.hpp"

#include "utils/logger.hpp"
#include "utils/ubiquity_time.h"

#include <arpa/inet.h>
#include <cstring>
#include <memory>
#include <atomic>

typedef struct node_client_s { 

    // Set at creation.
    ip4_t ip;
    char ip_string[INET_ADDRSTRLEN];
    std::atomic<uint64_t> weight;
    connection_status_t connection_status;
    ubiquity_time_t connection_status_expiration_time;
    connection_ref_t connection;
    // Tools.
    ubiquity_logger_t logger;
} node_client_t;


void node_client_init(
        node_client_t *const node_client,
        const ip4_t ip,
        const node_weight_t weight
        ) noexcept
{
    node_client->logger = ubiquity_logger_create(true);
    node_client->ip = ip;
    node_client_set_weight(node_client, weight);
    inet_ntop(
            AF_INET,
            &ip,
            node_client->ip_string,
            INET_ADDRSTRLEN
            );
    node_client->connection_status = CONNECTION_EMPTY;
    // Set the connection expiration time to now so that a new connection is created at first invocation.
    node_client->connection_status_expiration_time = 0;
    // Create a new connection.
    node_client->connection = nullptr;

    // The new node client is ready.
    LOG_DEBUG_2(node_client->logger, "event=node_client_create node_client={} ip={} msg=\"New node client created.\"",
            (void *)node_client,
            node_client->ip_string
            );
}

node_client_ref_t node_client_create(
        const ip4_t ip,
        const node_weight_t weight) noexcept 
{
    node_client_t *const client = new (std::nothrow) node_client_t{};
    if (!client) {
        return nullptr;
    }
    node_client_init(client, ip, weight);
    return std::shared_ptr<node_client_t>(client, node_client_delete);
}

ip4_t node_client_get_ip(
        const node_client_t *const node_client
        ) noexcept
{
    return node_client->ip;
}

computed_node_weight_t node_client_get_weight(
        const node_client_t *const node_client
        ) noexcept
{
    return std::atomic_load(&node_client->weight);
}

void node_client_set_weight(
        node_client_t *const node_client,
        const node_weight_t weight
        ) noexcept
{
    const computed_node_weight_t computed_weight = (uint64_t)((float)0xffffffffull * weight);
    const computed_node_weight_t positive_computed_weight = 0 == computed_weight ? 1 : computed_weight;

    //   FIXME: update the weight atomically
    std::atomic_store(&node_client->weight, positive_computed_weight);
}

void node_client_delete(
        node_client_t *const node_client
        ) noexcept
{
    LOG_DEBUG_2(node_client->logger, "event=node_client_delete node_client={} ip={}",
            (void *)node_client,
            node_client->ip_string
            ); 
    delete node_client;
}

void node_client_update_connection_status(
            node_client_t *const node_client,
            connection_status_t status
            ) noexcept
{
    // Update the connection status.
    node_client->connection_status = status;
    node_client->connection_status_expiration_time = ubiquity_time_get_from_now(CONNECTION_STATUS_GRACE_PERIOD);
}

connection_status_t node_client_get_connection_status(
            node_client_t *const node_client
            ) noexcept
{
    return node_client->connection_status;
}

void node_client_handle_connection_empty(
        node_client_t *const node_client
        )noexcept
{
    // Create a new connection.
    node_client->connection = connection_create(
            node_client->ip_string
            );
    node_client_update_connection_status(node_client, CONNECTION_CREATED);
}

void node_client_handle_connection_created(
        node_client_t *const node_client
        )noexcept
{
    connection_ref_t connection = node_client->connection;
    const bool is_connected = connection && connection_ensure_ready(
            connection.get()
            );
    if(is_connected){
        node_client_update_connection_status(node_client, CONNECTION_UP);
    }
    else if(ubiquity_time_is_past(node_client->connection_status_expiration_time)){
        node_client_update_connection_status(node_client, CONNECTION_DOWN);
    }
    // else we are still waiting for it to become active
}

void node_client_handle_connection_up(
        node_client_t *const node_client
        )noexcept
{
    connection_ref_t connection = node_client->connection;
    const bool is_connected = connection && connection_is_ready(
            connection.get()
            );
    if(is_connected){
        node_client_update_connection_status(node_client, CONNECTION_UP);
    }
    else {
        node_client_update_connection_status(node_client, CONNECTION_DOWN);
    }
    // else we are still waiting for it to become active    
}

void node_client_handle_connection_down(
        node_client_t *const node_client
        )noexcept
{
    connection_ref_t connection = node_client->connection;
    const bool is_connected = connection && connection_is_ready(
            connection.get()
            );
    if(is_connected){
        node_client_update_connection_status(node_client, CONNECTION_UP);
    }
    else if(ubiquity_time_is_past(node_client->connection_status_expiration_time)){
        node_client->connection = nullptr;

        if (connection) {
            LOG_DEBUG_2(node_client->logger, "event=node_client_expired_connection node_client={} ip={}",
                (void *)node_client,
                node_client->ip_string
                ); 
        }
        node_client_update_connection_status(node_client, CONNECTION_EMPTY);
    }
    // else we are still waiting for it to become definitely down or get back to up     
}


connection_status_t node_client_invoke(
        node_client_t *const node_client
        ) noexcept
{
     // Save connection status for later comparison.
    connection_status_t last_status = node_client_get_connection_status(node_client);

    // Notify and persist any change in the node client's connection status.
    switch (last_status)
    {
        case CONNECTION_EMPTY: 
            node_client_handle_connection_empty(node_client);
            break;
        case CONNECTION_CREATED: 
            node_client_handle_connection_created(node_client);
            break;      
        case CONNECTION_UP: 
            node_client_handle_connection_up(node_client);
            break;
        case CONNECTION_DOWN: 
            node_client_handle_connection_down(node_client);
            break;        
        default:
            break;
    }
    // Let the caller know whether the node client is connected or not.
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
    connection_ref_t connection = node_client->connection;
    if (!connection) 
            return;
    connection_send_message(
            connection.get(),
            stream_id,
            message_content,
            message_size,
            status_code
            );
}

void node_client_poll_message(
        node_client_t *const node_client,
        const stream_id_t stream_id,
        uint8_t *const destination_buffer,
        size_t *const message_length
        ) noexcept
{

    connection_ref_t connection = node_client->connection;
    
    *message_length = 0;
    if (!connection) 
        return;
    connection_poll_message(
            connection.get(),
            stream_id,
            destination_buffer,
            message_length
            );
}
