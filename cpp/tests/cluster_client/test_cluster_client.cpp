#include "components/cluster_client.hpp"

#include "ubiquity_test.hpp"

#include "common/common.hpp"

#include <openssl/evp.h>

/*
 * MOCKS
 */

/*
Shared state store
*/
typedef struct shared_state_store_s { 
} shared_state_store_t;

typedef struct std::shared_ptr<shared_state_store_s> shared_state_store_ref_t;

shared_state_store_ref_t store;
 

shared_state_store_ref_t shared_state_store_create(){
    return store;
}

void shared_state_store_clear(
        shared_state_store_t* store
        )
{
}  

/*
    Synchronizer.
*/

typedef struct synchronizer_s { 
} synchronizer_t;


synchronizer_t synchronizer;

synchronizer_t *synchronizer_create(
        const char *const cluster_id,
        shared_state_store_ref_t store
        ) noexcept
{
    return &synchronizer;
}

void synchronizer_delete(
        synchronizer_t *const synchronizer
        ) noexcept
{
}

void synchronizer_store_initial_topology(
        synchronizer_t *const synchronizer,
        cluster_info_t *cluster_info
        ) noexcept
{
}

void synchronizer_use_initial_topology(
        synchronizer_t *const synchronizer
        ) noexcept
{
}

void synchronizer_invoke(
        synchronizer_t *const synchronizer
        ) noexcept
{
}
    

/*
    Client handle.
*/

typedef struct client_handle_s { 
} client_handle_t;

client_handle_t client_handle;

client_handle_t *client_handle_create(
        cluster_client_t *const cluster_client,
        shared_state_store_ref_t store,
        const char *const cluster_id
        ) noexcept
{
    return &client_handle;
}

/*
    Web client.
*/

typedef struct web_client_s { 
} web_client_t;

web_client_t s_web_client;

web_client_t *web_client_create(
        const char *const cluster_id
        ) noexcept
{
    return &s_web_client;
}

void web_client_delete(
        web_client_t *const web_client
        ) noexcept
{
}

status_code_t web_client_get_cluster_info(
        web_client_t *const web_client,
        cluster_info_t *cluster_info,
        const std::string cluster_id
        ) {
    cluster_info->ip_count = 0;
    return STATUS_CODE_SUCCESS;
}

/*
 * TESTS
 */

int main(void) {
    ubiquity_test_function_t test_list[] = {
        0,
    };
    return ubiquity_test_all(test_list);
}


