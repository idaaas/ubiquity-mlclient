#pragma once

#include <stddef.h>

bool ubiquity_set_aeron_directory(
        const char *const directory_path
        ) noexcept;

void ubiquity_set_credentials(
        const char *const app_token
        ) noexcept;

void ubiquity_set_api_address(
        const char *const root
        ) noexcept;

void ubiquity_set_resource_string_in_cache(
        const char *const id,
        const char *const config_string
        ) noexcept;

typedef struct client_handle_s client_handle_t;

client_handle_t *ubiquity_get_client_handle(void) noexcept;

int32_t ubiquity_open_client(
        const char *const cluster_id
        ) noexcept;

void ubiquity_close_client(void) noexcept;

int64_t ubiquity_xxh3_64bits(
        const char *source
        ) noexcept;

void ubiquity_set_default_log_level(
        const size_t log_level
        ) noexcept;

void ubiquity_set_protocol_log_level(
        const size_t log_level
        ) noexcept;

void ubiquity_set_log_file_level(
        const char *const file_path,
        const size_t level
        ) noexcept;

void ubiquity_set_log_file_daily_rotation(
        const char *const file_path,
        const int32_t rotation_hour,
        const int32_t rotation_minute
        ) noexcept;

void ubiquity_set_log_file_max_size(
        const char *const file_path,
        const size_t max_file_size
        ) noexcept;

void ubiquity_set_log_file_max_archives(
        const char *const file_path,
        const size_t max_file_count
        ) noexcept;

void ubiquity_set_stdout_level(
        const size_t level
        ) noexcept;

void ubiquity_set_stderr_level(
        const size_t level
        ) noexcept;

void ubiquity_set_syslog_level(
        const size_t level
        ) noexcept;
