#include "ubiquity_client/Ubiquity.hpp"
#include "components/ubiquity.hpp"
#include "components/client_handle.hpp"

#include <xxh3.h>

const char *Ubiquity::getStatusString(
        Ubiquity::StatusCode statusCode
        ) noexcept
{
    return ubiquity_get_status_string(statusCode);
}

/*
 * MAIN CONFIGURATION
 */

// Deprecated.
void Ubiquity::setCredential(
        const char *appToken
        ) noexcept
{
    ubiquity_set_credentials(appToken);
}

void Ubiquity::setCredentials(
        const char *appToken
        ) noexcept
{
    ubiquity_set_credentials(appToken);
}

void Ubiquity::setAeronDirectory(
        const char *directoryPath
        ) noexcept
{
    ubiquity_set_aeron_directory(directoryPath);
}

void Ubiquity::setApiAddress(
        const char *address
        ) noexcept
{
    ubiquity_set_api_address(address);
}

void Ubiquity::setResourceStringInCache(
        const char *const id,
        const char *const resourceString
        ) noexcept
{
    ubiquity_set_resource_string_in_cache(id, resourceString);
}

/*
 * HANDLES AND CONNECTIONS
 */

Ubiquity::ClientHandle Ubiquity::getClientHandle(void) noexcept
{
    Ubiquity::ClientHandle client_handle = Ubiquity::ClientHandle();
    client_handle.pimpl = ubiquity_get_client_handle();
    return client_handle;
}

Ubiquity::StatusCode Ubiquity::openClient(
        const char *const clusterId
        ) noexcept
{
    return ubiquity_open_client(clusterId);
}

void Ubiquity::closeClient(void) noexcept
{
    return ubiquity_close_client();
}

int64_t Ubiquity::xxh3_64bits(
        const char *source
        ) noexcept
{
    return ubiquity_xxh3_64bits(source);
}

void Ubiquity::setDefaultLogLevel(
        const int32_t level
        ) noexcept
{
    ubiquity_set_default_log_level(level);
}

void Ubiquity::setProtocolLogLevel(
        const int32_t level
        ) noexcept
{
    ubiquity_set_protocol_log_level(level);
}

void Ubiquity::setLogFileLevel(
        const char *const path,
        const int32_t level
        ) noexcept
{
    ubiquity_set_log_file_level(path, level);
}

void Ubiquity::setLogFileDailyRotation(
        const char *const path,
        const int32_t rotation_hour,
        const int32_t rotation_minute
        ) noexcept
{
    ubiquity_set_log_file_daily_rotation(path, rotation_hour, rotation_minute);
}

void Ubiquity::setLogFileMaxSize(
        const char *const path,
        const int64_t max_size
        ) noexcept
{
    ubiquity_set_log_file_max_size(path, (uint64_t)max_size);
}

void Ubiquity::setLogFileMaxArchives(
        const char *const path,
        const int64_t max_archives
        ) noexcept
{
    ubiquity_set_log_file_max_archives(path, (uint64_t)max_archives);
}

void Ubiquity::setStdoutLevel(
        const int32_t level
        ) noexcept
{
    ubiquity_set_stdout_level(level);
}

void Ubiquity::setStderrLevel(
        const int32_t level
        ) noexcept
{
    ubiquity_set_stderr_level(level);
}

void Ubiquity::setSyslogLevel(
        const int32_t level
        ) noexcept
{
    ubiquity_set_syslog_level(level);
}
