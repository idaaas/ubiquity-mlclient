#include "common/common.hpp"
#include "utils/logger.hpp"

#include <stdbool.h>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>

typedef struct web_client_s {
    char cluster_id[CLUSTER_ID_MAX_LENGTH];
    std::string *resource_string;
    ubiquity_logger_t logger;
    std::map<std::string, std::string> *resource_cache;
} web_client_t;

web_client_t *web_client_create(
        const char *const cluster_id
        ) noexcept
{
    web_client_t *web_client =  new web_client_t();
    snprintf(web_client->cluster_id, CLUSTER_ID_MAX_LENGTH, "%s", cluster_id);
    // TODO check errors
    web_client->logger = ubiquity_logger_create(true);
    web_client->resource_cache = new std::map<std::string, std::string>{};
    return web_client;
}

void web_client_delete(
        web_client_t *const web_client
        ) noexcept
{
    delete web_client->resource_cache;
    delete web_client;
}

static size_t write_resource_string(
        void *buffer,
        size_t size,
        size_t nmemb,
        void *userp
        ) noexcept
{
    web_client_t *const web_client = (web_client_t *)userp;
    const size_t length = size * nmemb;
    try {
        web_client->resource_string->append(reinterpret_cast<char *>(buffer), length);
    } catch (std::length_error &e) {
        LOG_ERROR_0(web_client->logger, "event=write_resource_string msg=\"Web client : Received resource string is too long.\"");
        return 0;
    }
    return length;
}

static constexpr size_t CURL_OPERATION_TIME_LIMIT_MS = 30000L;

static std::string s_app_token;

static inline status_code_t fetch_resource_from_rest_api(
        web_client_t *const web_client,
        const std::string &url
        ) noexcept
{
    LOG_TRACE_1(web_client->logger, "event=fetch_resource_from_rest_api url={} msg=\"Web client : Starting to fetch resource.\"",
            url
            );

    web_client->resource_string->clear();

    CURL *curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_resource_string);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, web_client);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, CURL_OPERATION_TIME_LIMIT_MS);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, ("authorization: Bearer " + s_app_token).c_str());
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

    char curl_error_string[CURL_ERROR_SIZE];
    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error_string);
    CURLcode curl_error = curl_easy_perform(curl_handle);

    long http_status;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_status);

    curl_easy_cleanup(curl_handle);

    if (CURLE_OK != curl_error) {
        LOG_ERROR_3(web_client->logger, "event=fetch_resource_from_rest_api url={} curl_error={} curl_error_string={} msg=\"Web client : Curl request failed.\"",
                url,
                0 + curl_error,
                curl_error_string
                );
        if (CURLE_OPERATION_TIMEDOUT == curl_error) {
            return STATUS_CODE_NO_API_RESPONSE;
        } else {
            return STATUS_CODE_OTHER_FETCH_ERROR;
        }
    }
    switch (http_status) {
        case 200:
            break;
        case 404:
            return STATUS_CODE_UNKNOWN_RESOURCE;
        case 400:
            return STATUS_CODE_INVALID_RESOURCE_ID;
        case 401:
            return STATUS_CODE_UNAUTHORIZED;
        case 403:
            return STATUS_CODE_FORBIDDEN;
        case 500:
        case 502:
            return STATUS_CODE_INTERNAL_API_ERROR;
        default:
            return STATUS_CODE_OTHER_FETCH_ERROR;
    }
    if (200 != http_status) {
        LOG_ERROR_3(
                web_client->logger,
                "event=fetch_resource_from_rest_api cluster={} url={} http_status={} msg=\"Web client : Http error.\"",
                web_client->cluster_id,
                url,
                http_status
                );
    }
    return STATUS_CODE_SUCCESS;
}

static std::map<std::string, std::string> s_resource_cache;

static inline const std::string consume_resource_in_cache(
        const std::string &resource_id
        ) noexcept
{
    auto it = s_resource_cache.find(resource_id);
    if (s_resource_cache.end() != it) {
        auto resource = it->second;

        // The resource is consumed and must be downloaded again or set again.
        s_resource_cache.erase(it);
        return resource;
    }
    return std::string();
}

static inline status_code_t load_ip_list_from_description(
        cluster_info_t *const cluster_info,
        const char *configuration_string
        ) noexcept
{
    // Load cluster description from JSON string.
    nlohmann::json cluster_description;
    try {
        std::istringstream(configuration_string) >> cluster_description;
    } catch (nlohmann::json::exception &e) {
        return STATUS_CODE_INVALID_CLUSTER_DESCRIPTION;
    }
    const nlohmann::json &ip_string_array = cluster_description["contact_points"];
    if (!ip_string_array.is_array() || ip_string_array.size() == 0) {
        return STATUS_CODE_INVALID_CLUSTER_DESCRIPTION;
    }

    // Load IPs from string.
    uint32_t ip;
    cluster_info->ip_count = 0;
    for (const auto &ip_string : ip_string_array) {
        if (inet_pton(AF_INET, ip_string.get<std::string>().c_str(), &ip)) {

            // Description should contain at most MAXIMUM_NODE_COUNT IP.
            if (MAXIMUM_NODE_COUNT <= cluster_info->ip_count) {
                return STATUS_CODE_INVALID_CLUSTER_DESCRIPTION;
            }
            cluster_info->ip_list[cluster_info->ip_count++] = ip;
        }
    }

    // Description should contain at least one IP.
    if (0 == cluster_info->ip_count) {
        return STATUS_CODE_INVALID_CLUSTER_DESCRIPTION;
    }

    return STATUS_CODE_SUCCESS;
}

std::string s_api_url_root = std::string("https://api.ubiquity.ai/v1");

status_code_t web_client_get_cluster_info(
        web_client_t *const web_client,
        cluster_info_t *const cluster_info,
        const std::string cluster_id
        ) noexcept
{
    LOG_TRACE_1(
            web_client->logger,
            "event=consume_resource_in_cache cluster={} msg=\"Web client : Looking for string resource in cache.\"",
            web_client->cluster_id
            );
    std::string config_string = consume_resource_in_cache(web_client->cluster_id);
    if (config_string.empty()) {
        web_client->resource_string = &config_string;
        LOG_DEBUG_1(
                web_client->logger,
                "event=load_configuration cluster={} msg=\"Web client : Cluster information not in cache ; fetching it through the web API.\"",
                web_client->cluster_id
                );
        status_code_t status_code = fetch_resource_from_rest_api(
                web_client,
                s_api_url_root + "/clusters/" + web_client->cluster_id
                );
        if (STATUS_CODE_SUCCESS != status_code) {
            LOG_ERROR_2(
                    web_client->logger,
                    "event=get_cluster_info cluster={} status={} msg=\"Web client : Failed to fetch cluster information through the web API.\"",
                    web_client->cluster_id,
                    ubiquity_get_status_string(status_code)
                    );
            return status_code;
        }
    } else {
        LOG_DEBUG_1(
                web_client->logger,
                "event=load_configuration cluster={} msg=\"Web client : Cluster information consumed from cache.\"",
                web_client->cluster_id
                );
    }
    return load_ip_list_from_description(
            cluster_info,
            config_string.c_str()
            );
}

void web_client_set_api_credentials(
        const char *const app_token
        ) noexcept
{
    s_app_token = std::string(app_token);
}

void web_client_set_api_address(
        const char *address
        ) noexcept
{
    s_api_url_root = std::string("https://") + address + "/v1";
}

void web_client_set_resource_in_cache(
        const char *const id,
        const char *const resource_string
        ) noexcept
{
    s_resource_cache[std::string(id)] = std::string(resource_string);
}
