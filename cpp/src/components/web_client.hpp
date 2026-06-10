#pragma once

#include "common/common.hpp"

typedef struct web_client_s web_client_t;

/*
    Called by cluster_client.
*/

web_client_t *web_client_create(
        const char *const cluster_id
        ) noexcept;

void web_client_delete(
        web_client_t *const web_client
        ) noexcept;

status_code_t web_client_get_cluster_info(
        web_client_t *const web_client,
        cluster_info_t *const cluster_info,
        const std::string cluster_id
        ) noexcept;

/*
    Called by ubiquity.
*/

void web_client_set_api_credentials(
        const char *const app_token
        ) noexcept;

void web_client_set_api_address(
        const char *address
        ) noexcept;

void web_client_set_resource_in_cache(
        const char *const id,
        const char *const resource_string
        ) noexcept;
