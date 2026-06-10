#include "client_handle.hpp"
#include "cluster_client.hpp"
#include "connection.hpp"
#include "node_client.hpp"
#include "web_client.hpp"

#include "common/common.hpp"
#include "utils/prng.hpp"
#include "utils/logger.hpp"

#include "GitVersion.hpp"

#include <xxh3.h>

#include <cstddef>
#include <utility>
#include <atomic>
#include <mutex>
#include <thread>

/*
 * MAIN CONFIGURATION
 */

static inline bool is_big_endian(void) noexcept {
    union {
        uint32_t uint32;
        uint8_t uint8[4];
    } value = {1};
    return value.uint8[0] == 0;
}

static ubiquity_logger_t s_logger = UBIQUITY_LOGGER_NULL;

void ubiquity_set_credentials(
        const char *app_token
        ) noexcept
{
    if (UBIQUITY_LOGGER_NULL != s_logger) {
        LOG_DEBUG_0(
                s_logger,
                "event=set_api_credentials msg=\"Ubiquity : API credentials were set.\""
                );
    }
    web_client_set_api_credentials(app_token);
}

static bool s_initialized = false;

static cluster_client_t *s_cluster_client = NULL;

static inline status_code_t ubiquity_initialize(void) noexcept
{
    if (s_initialized) {
        return STATUS_CODE_SUCCESS;
    }
    s_logger = ubiquity_logger_create(true);
    LOG_INFO_1(s_logger, "event=ubiquity_initialize build={} msg=\"Ubiquity : Starting initialization.\"",
            kGitHash
            );

    if (is_big_endian()) {
        LOG_CRITICAL_1(s_logger, "event=ubiquity_initialize status={} msg=\"Ubiquity : Big endian architectures are not supported.\"",
                ubiquity_get_status_string(STATUS_CODE_ARCHITECTURE_NOT_SUPPORTED)
                );
        return STATUS_CODE_ARCHITECTURE_NOT_SUPPORTED;
    }

    if (!connection_initialize_module()) {
        LOG_CRITICAL_1(s_logger, "event=ubiquity_initialize status={} msg=\"Ubiquity : No connection module available.\"",
                ubiquity_get_status_string(STATUS_CODE_AERON_INITIALIZATION_FAILED)
                );
        return STATUS_CODE_ARCHITECTURE_NOT_SUPPORTED;
    }

    // Sleep for a random duration between 0 and 2 seconds in order to desynchronize clients.
    LOG_INFO_0(s_logger, "event=ubiquity_initialize msg=\"Ubiquity : Sleeping for 0-2 seconds to desynchronize clients.\"");
    std::this_thread::sleep_for(std::chrono::milliseconds(ubiquity_prng_generate_once() % 2000));

    // The ubiquity client is now initialized.
    s_initialized = true;
    LOG_INFO_1(s_logger, "event=ubiquity_initialize status={} msg=\"Ubiquity :  Initialized successfully.\"",
            ubiquity_get_status_string(STATUS_CODE_SUCCESS)
            );

    return STATUS_CODE_SUCCESS;
}

bool ubiquity_set_aeron_directory(
        const char *const directory_path
        ) noexcept
{
    return connection_set_directory(directory_path);
}

void ubiquity_set_api_address(
        const char *address
        ) noexcept
{
    if (UBIQUITY_LOGGER_NULL != s_logger) {
        LOG_INFO_1(
                s_logger,
                "event=set_api_address address={} msg=\"Ubiquity : Custom API address was set.\"",
                address
                );
    }
    web_client_set_api_address(address);
}

void ubiquity_set_resource_string_in_cache(
        const char *const id,
        const char *const resource_string
        ) noexcept
{
    if (UBIQUITY_LOGGER_NULL != s_logger) {
        LOG_INFO_1(
                s_logger,
                "event=set_resource_in_cache id={} msg=\"Ubiquity : Resource manually set in cache.\"",
                id
                );
    }
    web_client_set_resource_in_cache(id, resource_string);
}

/*
 * HANDLES AND CONNECTIONS
 */

static std::mutex s_cluster_clients_mutex;

client_handle_t *ubiquity_get_client_handle(void) noexcept
{
    std::lock_guard<std::mutex> guard(s_cluster_clients_mutex);

    return cluster_client_create_handle(s_cluster_client);
}

status_code_t ubiquity_open_client(
        const char *const cluster_id
        ) noexcept
{
    std::lock_guard<std::mutex> guard(s_cluster_clients_mutex);

    status_code_t status_code;

    // If there is already an open cluster client, leave.
    if (NULL != s_cluster_client) {
        return STATUS_CODE_SUCCESS;
    }

    // Initialize the whole Ubiquity client.
    status_code = ubiquity_initialize();
    if (STATUS_CODE_SUCCESS != status_code) {
        return status_code;
    }

    // Create the cluster client.
    s_cluster_client = cluster_client_create(
            cluster_id,
            &status_code
            );
    return status_code;
}

void ubiquity_close_client(void) noexcept
{
    std::lock_guard<std::mutex> guard(s_cluster_clients_mutex);

    if (NULL != s_cluster_client) {
        cluster_client_allow_termination(s_cluster_client);
    }
}

static inline void force_close_client(void) noexcept
{
    std::lock_guard<std::mutex> guard(s_cluster_clients_mutex);

    if (NULL != s_cluster_client) {
        cluster_client_force_termination(s_cluster_client);
    }
}

struct gets_called_on_exit_t {
    ~gets_called_on_exit_t(void) {
        if (s_initialized) {
            force_close_client();
            connection_close_module();
        }
    }
} gets_called_on_exit{};

int64_t ubiquity_xxh3_64bits(
        const char *source
        ) noexcept
{
    return XXH3_64bits(source, strlen(source));
}

void ubiquity_set_default_log_level(
        const ubiquity_log_level_t log_level
        ) noexcept
{
    ubiquity_logger_set_default_level(log_level);
}

void ubiquity_set_protocol_log_level(
        const ubiquity_log_level_t log_level
        ) noexcept
{
    ubiquity_logger_set_protocol_log_level(log_level);
}

void ubiquity_set_log_file_level(
        const char *const file_path,
        const ubiquity_log_level_t level
        ) noexcept
{
    ubiquity_logger_set_file_sink_level(file_path, level);
}

void ubiquity_set_log_file_daily_rotation(
        const char *const file_path,
        const int32_t rotation_hour,
        const int32_t rotation_minute
        ) noexcept
{
    ubiquity_logger_set_file_sink_daily_rotation(file_path, rotation_hour, rotation_minute);
}

void ubiquity_set_log_file_max_size(
        const char *const file_path,
        const size_t max_file_size
        ) noexcept
{
    ubiquity_logger_set_file_sink_max_size(file_path, max_file_size);
}

void ubiquity_set_log_file_max_archives(
        const char *const file_path,
        const size_t max_file_count
        ) noexcept
{
    ubiquity_logger_set_file_sink_max_archives(file_path, max_file_count);
}

void ubiquity_set_stdout_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    ubiquity_logger_set_stdout_level(level);
}

void ubiquity_set_stderr_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    ubiquity_logger_set_stderr_level(level);
}

void ubiquity_set_syslog_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    ubiquity_logger_set_syslog_level(level);
}
