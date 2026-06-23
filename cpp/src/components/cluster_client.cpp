#include "cluster_client.hpp"

#include "shared_state_store.hpp"
#include "client_handle.hpp"
#include "synchronizer.hpp"
#include "web_client.hpp"

#include "utils/logger.hpp"
#include "utils/ubiquity_time.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <cstddef>
#include <cstring>

#define TOPOLOGY_VALIDITY_PERIOD UBIQUITY_TIME_SECONDS(30)

#define NODE_CLIENT_LIST_SIZE (MAXIMUM_NODE_COUNT * 2)

typedef struct cluster_client_s {
    char cluster_id[CLUSTER_ID_MAX_LENGTH];
    std::thread thread;
    std::atomic<size_t> use_count;

    web_client_t *web_client;
    std::shared_ptr<shared_state_store_t> store;
    synchronizer_t *synchronizer;
    ubiquity_logger_t logger;

    ubiquity_time_t topology_expiration_time;

    // Cluster topology.
    bool has_valid_topology;

    // Disconnection and termination.
    std::atomic<bool> is_connected;
    std::atomic<bool> is_forced_to_terminate;
    std::atomic<bool> can_terminate;
    bool is_gracefully_terminating;
} cluster_client_t;

static inline status_code_t cluster_client_load_topology(
        cluster_client_t *const cluster_client
        ) noexcept
{
    LOG_DEBUG_1(
            cluster_client->logger,
            "event=load_topology cluster={} msg=\"Cluster client : Attempting to fetch and load cluster topology.\"",
            cluster_client->cluster_id
            );

    // Get cluster information from the web client.
    cluster_info_t cluster_info = {};
    const status_code_t status_code = web_client_get_cluster_info(
            cluster_client->web_client,
            &cluster_info,
            cluster_client->cluster_id
            );

    // If the new topology was successfully fetched and loaded, there's nothing more to do.
    if (STATUS_CODE_SUCCESS == status_code
            && synchronizer_store_initial_topology(
                    cluster_client->synchronizer,
                    &cluster_info
                    )) {
        cluster_client->has_valid_topology = true;
        synchronizer_use_initial_topology(cluster_client->synchronizer);

        // A valid topology was loaded, so the topology expiration time is refreshed.
        cluster_client->topology_expiration_time = ubiquity_time_get_from_now(TOPOLOGY_VALIDITY_PERIOD);
        return status_code;
    }

    // The new topology might be unfetchable or unloadable.
    if (STATUS_CODE_SUCCESS != status_code) {
        LOG_WARNING_1(
                cluster_client->logger,
                "event=load_topology cluster={} status=UNFETCHABLE_CLUSTER_TOPOLOGY msg=\"Failed to fetch cluster topology through the web API. Attempting to use the last known topology as a fallback.\"",
                cluster_client->cluster_id
                );
    } else {
        LOG_WARNING_1(
                cluster_client->logger,
                "event=load_topology cluster={} status=UNLOADABLE_CLUSTER_TOPOLOGY sub_status=INVALID_CLUSTER_TOPOLOGY msg=\"The cluster topology fetched through the web API is invalid. Attempting to use the last known topology as a fallback.\"",
                cluster_client->cluster_id
                );
    }

    // If fetching and loading a new topology is not possible, attempt to use the last known valid topology as a fallback.
    if (cluster_client->has_valid_topology) {
        synchronizer_use_initial_topology(cluster_client->synchronizer);

        // A valid topology was loaded, so the topology expiration time is refreshed.
        cluster_client->topology_expiration_time = ubiquity_time_get_from_now(TOPOLOGY_VALIDITY_PERIOD);
        return STATUS_CODE_SUCCESS;
    }

    // If fetching and loading a new topology failed and there is no known valid topology to use as a fallback, the whole topology loading failed.
    LOG_ERROR_1(
            cluster_client->logger,
            "event=load_topology cluster={} status=NO_CLUSTER_TOPOLOGY msg=\"Failed to fetch cluster topology through the web API or failed to load it.\" No known topology to use as a fallback.",
            cluster_client->cluster_id
            );
    return status_code;
}

static inline bool cluster_client_is_forced_to_terminate(
        cluster_client_t *const cluster_client
        )
{
    return std::atomic_load(&cluster_client->is_forced_to_terminate);
}

static inline bool cluster_client_can_terminate(
        cluster_client_t *const cluster_client
        )
{
    return std::atomic_load(&cluster_client->can_terminate);
}

static inline bool cluster_client_has_no_handle_left(
        cluster_client_t *const cluster_client
        ) noexcept
{
    return 0 == std::atomic_load(&cluster_client->use_count);
}

static inline bool cluster_client_delete(
        cluster_client_t *const cluster_client
        ) noexcept
{
    shared_state_store_clear(cluster_client->store.get());
    synchronizer_delete(cluster_client->synchronizer);
    web_client_delete(cluster_client->web_client);
    cluster_client->thread.detach();
    delete cluster_client;
    return true;
}

static void cluster_client_thread_function(
        cluster_client_t *const cluster_client,
        std::atomic<bool>* termination_done_flag
        ) noexcept
{
    LOG_INFO_1(
            cluster_client->logger,
            "event=ubiquity_open_client cluster={} msg=\"Ubiquity : Starting client.\"",
            cluster_client->cluster_id
            );
    cluster_client->is_gracefully_terminating = false;

    // Work until cluster client termination.
    while (!cluster_client_is_forced_to_terminate(cluster_client)
            && !cluster_client->is_gracefully_terminating) {

        // Synchronize topology, model shardings and metadata with the cluster.
        const size_t connected_nc_count = synchronizer_invoke(cluster_client->synchronizer);

        // The cluster client is connected if at least one node client is connected.
        cluster_client->is_connected.store(0 < connected_nc_count);

        // If the cluster client is connected, the topology validity period is expanded.
        if (std::atomic_load(&cluster_client->is_connected)) {
            cluster_client->topology_expiration_time = ubiquity_time_get_from_now(TOPOLOGY_VALIDITY_PERIOD);
        }

        // If the topology is expired, load a fresh topology the web API or reload the last known valid one.
        if (ubiquity_time_is_past(cluster_client->topology_expiration_time)
                && STATUS_CODE_SUCCESS != cluster_client_load_topology(cluster_client)) {

            // No valid topology found. Terminate the cluster client.
            break;
        }

        // If the user called for the cluster client's termination and all handles are closed, the thread can terminate gracefully.
        if (!cluster_client->is_gracefully_terminating
                && cluster_client_has_no_handle_left(cluster_client)
                && cluster_client_can_terminate(cluster_client)) {
            LOG_INFO_1(
                    cluster_client->logger,
                    "event=unbind_all_clients cluster={} msg=\"Cluster client : Unbinding all node clients before termination.\"",
                    cluster_client->cluster_id
                    );
            cluster_client->is_gracefully_terminating = true;
        }

        // Don't iterate more than once every millisecond.
        ubiquity_time_sleep(UBIQUITY_TIME_MILLISECONDS(1));
    }

    LOG_INFO_1(
            cluster_client->logger,
            "event=terminate_thread cluster={} msg=\"Cluster client : Terminating dedicated thread.\"",
            cluster_client->cluster_id
            );

    // Stop synchronizer thread and empty shared state store.
    cluster_client_delete(cluster_client);

    // Signal that this thread is fully done,
    termination_done_flag->store(true, std::memory_order_release);
}

/*
 * Called by ubiquity.cpp
 */

#define CLUSTER_FIRST_CONNECTION_TIMEOUT UBIQUITY_TIME_SECONDS(30)


bool cluster_client_is_connected(
        cluster_client_t *const cluster_client
        ) noexcept
{
    return std::atomic_load(&cluster_client->is_connected);
}

cluster_client_t *cluster_client_create(
        const char *const cluster_id,
        std::atomic<bool>* termination_done_flag,
        status_code_t *const status_code
        ) noexcept
{
    ubiquity_time_t timeout;
    *status_code = STATUS_CODE_OUT_OF_MEMORY;

    // Create the cluster client.
    cluster_client_t *const cluster_client = new (std::nothrow) cluster_client_t{}; 
    if (NULL == cluster_client) {
        goto initialization_failure_1;
    }
    strcpy(cluster_client->cluster_id, cluster_id);
    std::atomic_store(&cluster_client->use_count, 0);

    // Create the logger.
    cluster_client->logger = ubiquity_logger_create(true);
    if (UBIQUITY_LOGGER_NULL == cluster_client->logger) {
        goto initialization_failure_2;
    }

    // Create the web client.
    cluster_client->web_client = web_client_create(cluster_id);
    if (NULL == cluster_client->web_client) {
        goto initialization_failure_3;
    }

    // Create the shared store
    cluster_client->store =  shared_state_store_create();
    if (NULL == cluster_client->store) {
        goto initialization_failure_5;        
    }

    // Create the synchronizer.
    cluster_client->synchronizer = synchronizer_create(cluster_id, cluster_client->store);
    if (NULL == cluster_client->synchronizer) {
        goto initialization_failure_5;
    }

    // Mark the topology as not received yet.
    cluster_client->has_valid_topology = false;

    // Load the initial cluster client's topology.
    *status_code = cluster_client_load_topology(cluster_client);
    if (STATUS_CODE_SUCCESS != *status_code) {
        goto initialization_failure_6;
    }

    // Create the cluster client's dedicated thread.
    cluster_client->thread = std::thread(
            &cluster_client_thread_function,
            cluster_client, 
            termination_done_flag
            );

    // Wait for the cluster to be connected before returning the cluster_client.
    timeout = ubiquity_time_get_from_now(CLUSTER_FIRST_CONNECTION_TIMEOUT);
    bool is_connected;
    do {
        ubiquity_time_sleep(UBIQUITY_TIME_MILLISECONDS(1));
        is_connected = cluster_client_is_connected(cluster_client);
    }
    while (!is_connected && !ubiquity_time_is_past(timeout));

    if (is_connected) {
        *status_code = STATUS_CODE_SUCCESS;
        return cluster_client;
    }
    
    *status_code = STATUS_CODE_CONNECTION_TIMEOUT;

    // connection timed out, send termination signal and wait for thread to join before exiting
    cluster_client_force_termination(cluster_client);
    cluster_client->thread.join();
    goto initialization_failure_1;

    initialization_failure_6:
        synchronizer_delete(cluster_client->synchronizer);
    initialization_failure_5:
        web_client_delete(cluster_client->web_client);
    initialization_failure_3:
        ubiquity_logger_delete(cluster_client->logger);
    initialization_failure_2:
        delete cluster_client;
    initialization_failure_1:
         return NULL;
}

client_handle_t *cluster_client_create_handle(
        cluster_client_t *const cluster_client
        ) noexcept
{
    return client_handle_create(
            cluster_client,
            cluster_client->store,
            cluster_client->cluster_id
            );
}

void cluster_client_allow_termination(
        cluster_client_t *const cluster_client
        ) noexcept
{
    LOG_INFO_1(cluster_client->logger, "event=close_client cluster={} msg=\"Ubiquity : Authorizing cluster client termination.\"",
            cluster_client->cluster_id
            );
    std::atomic_store(&cluster_client->can_terminate, true);
}

void cluster_client_force_termination(
        cluster_client_t *const cluster_client
        ) noexcept
{
    LOG_INFO_1(cluster_client->logger, "event=force_close_client cluster={} msg=\"Ubiquity : Forcing cluster client termination.\"",
            cluster_client->cluster_id
            );
    std::atomic_store(&cluster_client->is_forced_to_terminate, true);
}

/*
 * Called by client_handle.cpp
 */

void cluster_client_increment_use_count(
        cluster_client_t *const cluster_client
        ) noexcept
{
    std::atomic_store(&cluster_client->use_count, cluster_client->use_count + 1);
}

void cluster_client_decrement_use_count(
        cluster_client_t *const cluster_client
        ) noexcept
{
    std::atomic_store(&cluster_client->use_count, cluster_client->use_count - 1);
}
