#pragma once

#include "logger_config.h"

#include <stddef.h>

enum ubiquity_log_level_e {
    UBIQUITY_LOG_OFF = 0,

    UBIQUITY_LOG_TRACE = 0x01,
    UBIQUITY_LOG_DEBUG = 0x02,
    UBIQUITY_LOG_INFO = 0x04,
    UBIQUITY_LOG_WARNING = 0x08,
    UBIQUITY_LOG_ERROR = 0x10,
    UBIQUITY_LOG_CRITICAL = 0x20,

    UBIQUITY_MIN_LOG_TRACE = 0x3f,
    UBIQUITY_MIN_LOG_DEBUG = 0x3e,
    UBIQUITY_MIN_LOG_INFO = 0x3c,
    UBIQUITY_MIN_LOG_WARNING = 0x38,
    UBIQUITY_MIN_LOG_ERROR = 0x30,
    UBIQUITY_MIN_LOG_CRITICAL = 0x20,

    UBIQUITY_MAX_LOG_TRACE = 0x01,
    UBIQUITY_MAX_LOG_DEBUG = 0x03,
    UBIQUITY_MAX_LOG_INFO = 0x07,
    UBIQUITY_MAX_LOG_WARNING = 0x0f,
    UBIQUITY_MAX_LOG_ERROR = 0x1f,
    UBIQUITY_MAX_LOG_CRITICAL = 0x3f,

    UBIQUITY_LOG_NONE = 0xffff,
};

#if defined(NO_LOG)

#define UBIQUITY_LOGGER_NULL 0

typedef size_t ubiquity_log_level_t;

typedef int ubiquity_logger_t;

static void ubiquity_logger_set_default_level(
        const size_t log_level
        ) noexcept
{}

static void ubiquity_logger_set_file_sink_level(
        const char *const file_path,
        const ubiquity_log_level_t level
        ) noexcept
{}

static void ubiquity_logger_set_file_sink_daily_rotation(
        const char *const file_path,
        const int rotation_hour,
        const int rotation_minute
        ) noexcept
{}

static void ubiquity_logger_set_file_sink_max_size(
        const char *const file_path,
        const size_t max_file_size
        ) noexcept
{}

static void ubiquity_logger_set_file_sink_max_archives(
        const char *const file_path,
        const size_t max_file_count
        ) noexcept
{}

static void ubiquity_logger_set_stdout_level(
        const ubiquity_log_level_t level
        ) noexcept
{}

static void ubiquity_logger_set_stderr_level(
        const ubiquity_log_level_t level
        ) noexcept
{}

static void ubiquity_logger_set_syslog_level(
        const ubiquity_log_level_t level
        ) noexcept
{}

static ubiquity_logger_t ubiquity_logger_create(
        const bool is_protocol
        ) noexcept
{
    return 0;
}

static void ubiquity_logger_delete(
        ubiquity_logger_t logger
        ) noexcept
{}

static void ubiquity_logger_set_level(
        ubiquity_logger_t logger,
        const ubiquity_log_level_t level
        ) noexcept
{}

#else

#define UBIQUITY_LOGGER_NULL NULL

#include <memory>

#include "spdlog/spdlog.h"

typedef size_t ubiquity_log_level_t;

static inline ubiquity_log_level_t get_ubiquity_log_level(
        const spdlog::level::level_enum level
        ) noexcept
{
    switch (level) {
        case spdlog::level::off:
            return UBIQUITY_LOG_OFF;

        case spdlog::level::trace:
            return UBIQUITY_LOG_TRACE;

        case spdlog::level::debug:
            return UBIQUITY_LOG_DEBUG;

        case spdlog::level::info:
            return UBIQUITY_LOG_INFO;

        case spdlog::level::warn:
            return UBIQUITY_LOG_WARNING;

        case spdlog::level::err:
            return UBIQUITY_LOG_ERROR;

        case spdlog::level::critical:
            return UBIQUITY_LOG_CRITICAL;

        default:
            return UBIQUITY_LOG_OFF;
    }
}

class filtered_logger_t : public spdlog::logger {
    ubiquity_log_level_t level;

public:
    filtered_logger_t(
            const std::string& name,
            const ubiquity_log_level_t level,
            const std::vector<spdlog::sink_ptr>& sinks
            ) : spdlog::logger(name, sinks.begin(), sinks.end()), level(level)
    {
        spdlog::logger::set_level(spdlog::level::trace);
    }

    void set_filter_level(
            ubiquity_log_level_t level
            )
    {
        this->level = level;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& message) override {
        if (get_ubiquity_log_level(message.level) & this->level) {
            spdlog::logger::sink_it_(message);
        }
    }
};

typedef std::shared_ptr<filtered_logger_t> ubiquity_logger_t;

void ubiquity_logger_set_default_level(
        const size_t log_level
        ) noexcept;

void ubiquity_logger_set_file_sink_level(
        const char *const file_path,
        const ubiquity_log_level_t level
        ) noexcept;

void ubiquity_logger_set_file_sink_daily_rotation(
        const char *const file_path,
        const int rotation_hour,
        const int rotation_minute
        ) noexcept;

void ubiquity_logger_set_file_sink_max_size(
        const char *const file_path,
        const size_t max_file_size
        ) noexcept;

void ubiquity_logger_set_file_sink_max_archives(
        const char *const file_path,
        const size_t max_file_count
        ) noexcept;

void ubiquity_logger_set_stdout_level(
        const ubiquity_log_level_t level
        ) noexcept;

void ubiquity_logger_set_stderr_level(
        const ubiquity_log_level_t level
        ) noexcept;

void ubiquity_logger_set_syslog_level(
        const ubiquity_log_level_t level
        ) noexcept;

void ubiquity_logger_set_protocol_log_level(
        const ubiquity_log_level_t level
        ) noexcept;

ubiquity_logger_t ubiquity_logger_create(
        const bool is_protocol
        ) noexcept;

void ubiquity_logger_delete(
        ubiquity_logger_t logger
        ) noexcept;

void ubiquity_logger_set_level(
        ubiquity_logger_t logger,
        const ubiquity_log_level_t level
        ) noexcept;
#endif

#if defined(NDEBUG) || defined(NO_LOG)

#define LOG_TRACE_0(logger, format)
#define LOG_TRACE_1(logger, format, a1)
#define LOG_TRACE_2(logger, format, a1, a2)
#define LOG_TRACE_3(logger, format, a1, a2, a3)
#define LOG_TRACE_4(logger, format, a1, a2, a3, a4)
#define LOG_TRACE_5(logger, format, a1, a2, a3, a4, a5)
#define LOG_TRACE_6(logger, format, a1, a2, a3, a4, a5, a6)
#define LOG_TRACE_7(logger, format, a1, a2, a3, a4, a5, a6, a7)
#define LOG_TRACE_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8)
#define LOG_TRACE_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#define LOG_DEBUG_0(logger, format)
#define LOG_DEBUG_1(logger, format, a1)
#define LOG_DEBUG_2(logger, format, a1, a2)
#define LOG_DEBUG_3(logger, format, a1, a2, a3)
#define LOG_DEBUG_4(logger, format, a1, a2, a3, a4)
#define LOG_DEBUG_5(logger, format, a1, a2, a3, a4, a5)
#define LOG_DEBUG_6(logger, format, a1, a2, a3, a4, a5, a6)
#define LOG_DEBUG_7(logger, format, a1, a2, a3, a4, a5, a6, a7)
#define LOG_DEBUG_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8)
#define LOG_DEBUG_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#else

#define LOG_TRACE_0(logger, format) (logger->trace(format))
#define LOG_TRACE_1(logger, format, a1) (logger->trace(format, a1))
#define LOG_TRACE_2(logger, format, a1, a2) (logger->trace(format, a1, a2))
#define LOG_TRACE_3(logger, format, a1, a2, a3) (logger->trace(format, a1, a2, a3))
#define LOG_TRACE_4(logger, format, a1, a2, a3, a4) (logger->trace(format, a1, a2, a3, a4))
#define LOG_TRACE_5(logger, format, a1, a2, a3, a4, a5) (logger->trace(format, a1, a2, a3, a4, a5))
#define LOG_TRACE_6(logger, format, a1, a2, a3, a4, a5, a6) (logger->trace(format, a1, a2, a3, a4, a5, a6))
#define LOG_TRACE_7(logger, format, a1, a2, a3, a4, a5, a6, a7) (logger->trace(format, a1, a2, a3, a4, a5, a6, a7))
#define LOG_TRACE_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8) (logger->trace(format, a1, a2, a3, a4, a5, a6, a7, a8))
#define LOG_TRACE_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9) (logger->trace(format, a1, a2, a3, a4, a5, a6, a7, a8, a9))

#define LOG_DEBUG_0(logger, format) (logger->debug(format))
#define LOG_DEBUG_1(logger, format, a1) (logger->debug(format, a1))
#define LOG_DEBUG_2(logger, format, a1, a2) (logger->debug(format, a1, a2))
#define LOG_DEBUG_3(logger, format, a1, a2, a3) (logger->debug(format, a1, a2, a3))
#define LOG_DEBUG_4(logger, format, a1, a2, a3, a4) (logger->debug(format, a1, a2, a3, a4))
#define LOG_DEBUG_5(logger, format, a1, a2, a3, a4, a5) (logger->debug(format, a1, a2, a3, a4, a5))
#define LOG_DEBUG_6(logger, format, a1, a2, a3, a4, a5, a6) (logger->debug(format, a1, a2, a3, a4, a5, a6))
#define LOG_DEBUG_7(logger, format, a1, a2, a3, a4, a5, a6, a7) (logger->debug(format, a1, a2, a3, a4, a5, a6, a7))
#define LOG_DEBUG_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8) (logger->debug(format, a1, a2, a3, a4, a5, a6, a7, a8))
#define LOG_DEBUG_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9) (logger->debug(format, a1, a2, a3, a4, a5, a6, a7, a8, a9))

#endif

#if defined(NO_LOG)

#define LOG_INFO_0(logger, format)
#define LOG_INFO_1(logger, format, a1)
#define LOG_INFO_2(logger, format, a1, a2)
#define LOG_INFO_3(logger, format, a1, a2, a3)
#define LOG_INFO_4(logger, format, a1, a2, a3, a4)
#define LOG_INFO_5(logger, format, a1, a2, a3, a4, a5)
#define LOG_INFO_6(logger, format, a1, a2, a3, a4, a5, a6)
#define LOG_INFO_7(logger, format, a1, a2, a3, a4, a5, a6, a7)
#define LOG_INFO_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8)
#define LOG_INFO_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#define LOG_WARNING_0(logger, format)
#define LOG_WARNING_1(logger, format, a1)
#define LOG_WARNING_2(logger, format, a1, a2)
#define LOG_WARNING_3(logger, format, a1, a2, a3)
#define LOG_WARNING_4(logger, format, a1, a2, a3, a4)
#define LOG_WARNING_5(logger, format, a1, a2, a3, a4, a5)
#define LOG_WARNING_6(logger, format, a1, a2, a3, a4, a5, a6)
#define LOG_WARNING_7(logger, format, a1, a2, a3, a4, a5, a6, a7)
#define LOG_WARNING_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8)
#define LOG_WARNING_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#define LOG_ERROR_0(logger, format)
#define LOG_ERROR_1(logger, format, a1)
#define LOG_ERROR_2(logger, format, a1, a2)
#define LOG_ERROR_3(logger, format, a1, a2, a3)
#define LOG_ERROR_4(logger, format, a1, a2, a3, a4)
#define LOG_ERROR_5(logger, format, a1, a2, a3, a4, a5)
#define LOG_ERROR_6(logger, format, a1, a2, a3, a4, a5, a6)
#define LOG_ERROR_7(logger, format, a1, a2, a3, a4, a5, a6, a7)
#define LOG_ERROR_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8)
#define LOG_ERROR_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#define LOG_CRITICAL_0(logger, format)
#define LOG_CRITICAL_1(logger, format, a1)
#define LOG_CRITICAL_2(logger, format, a1, a2)
#define LOG_CRITICAL_3(logger, format, a1, a2, a3)
#define LOG_CRITICAL_4(logger, format, a1, a2, a3, a4)
#define LOG_CRITICAL_5(logger, format, a1, a2, a3, a4, a5)
#define LOG_CRITICAL_6(logger, format, a1, a2, a3, a4, a5, a6)
#define LOG_CRITICAL_7(logger, format, a1, a2, a3, a4, a5, a6, a7)
#define LOG_CRITICAL_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8)
#define LOG_CRITICAL_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9)

#else

#define LOG_INFO_0(logger, format) (logger->info(format))
#define LOG_INFO_1(logger, format, a1) (logger->info(format, a1))
#define LOG_INFO_2(logger, format, a1, a2) (logger->info(format, a1, a2))
#define LOG_INFO_3(logger, format, a1, a2, a3) (logger->info(format, a1, a2, a3))
#define LOG_INFO_4(logger, format, a1, a2, a3, a4) (logger->info(format, a1, a2, a3, a4))
#define LOG_INFO_5(logger, format, a1, a2, a3, a4, a5) (logger->info(format, a1, a2, a3, a4, a5))
#define LOG_INFO_6(logger, format, a1, a2, a3, a4, a5, a6) (logger->info(format, a1, a2, a3, a4, a5, a6))
#define LOG_INFO_7(logger, format, a1, a2, a3, a4, a5, a6, a7) (logger->info(format, a1, a2, a3, a4, a5, a6, a7))
#define LOG_INFO_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8) (logger->info(format, a1, a2, a3, a4, a5, a6, a7, a8))
#define LOG_INFO_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9) (logger->info(format, a1, a2, a3, a4, a5, a6, a7, a8, a9))

#define LOG_WARNING_0(logger, format) (logger->warn(format))
#define LOG_WARNING_1(logger, format, a1) (logger->warn(format, a1))
#define LOG_WARNING_2(logger, format, a1, a2) (logger->warn(format, a1, a2))
#define LOG_WARNING_3(logger, format, a1, a2, a3) (logger->warn(format, a1, a2, a3))
#define LOG_WARNING_4(logger, format, a1, a2, a3, a4) (logger->warn(format, a1, a2, a3, a4))
#define LOG_WARNING_5(logger, format, a1, a2, a3, a4, a5) (logger->warn(format, a1, a2, a3, a4, a5))
#define LOG_WARNING_6(logger, format, a1, a2, a3, a4, a5, a6) (logger->warn(format, a1, a2, a3, a4, a5, a6))
#define LOG_WARNING_7(logger, format, a1, a2, a3, a4, a5, a6, a7) (logger->warn(format, a1, a2, a3, a4, a5, a6, a7))
#define LOG_WARNING_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8) (logger->warn(format, a1, a2, a3, a4, a5, a6, a7, a8))
#define LOG_WARNING_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9) (logger->warn(format, a1, a2, a3, a4, a5, a6, a7, a8, a9))

#define LOG_ERROR_0(logger, format) (logger->error(format))
#define LOG_ERROR_1(logger, format, a1) (logger->error(format, a1))
#define LOG_ERROR_2(logger, format, a1, a2) (logger->error(format, a1, a2))
#define LOG_ERROR_3(logger, format, a1, a2, a3) (logger->error(format, a1, a2, a3))
#define LOG_ERROR_4(logger, format, a1, a2, a3, a4) (logger->error(format, a1, a2, a3, a4))
#define LOG_ERROR_5(logger, format, a1, a2, a3, a4, a5) (logger->error(format, a1, a2, a3, a4, a5))
#define LOG_ERROR_6(logger, format, a1, a2, a3, a4, a5, a6) (logger->error(format, a1, a2, a3, a4, a5, a6))
#define LOG_ERROR_7(logger, format, a1, a2, a3, a4, a5, a6, a7) (logger->error(format, a1, a2, a3, a4, a5, a6, a7))
#define LOG_ERROR_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8) (logger->error(format, a1, a2, a3, a4, a5, a6, a7, a8))
#define LOG_ERROR_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9) (logger->error(format, a1, a2, a3, a4, a5, a6, a7, a8, a9))

#define LOG_CRITICAL_0(logger, format) (logger->critical(format))
#define LOG_CRITICAL_1(logger, format, a1) (logger->critical(format, a1))
#define LOG_CRITICAL_2(logger, format, a1, a2) (logger->critical(format, a1, a2))
#define LOG_CRITICAL_3(logger, format, a1, a2, a3) (logger->critical(format, a1, a2, a3))
#define LOG_CRITICAL_4(logger, format, a1, a2, a3, a4) (logger->critical(format, a1, a2, a3, a4))
#define LOG_CRITICAL_5(logger, format, a1, a2, a3, a4, a5) (logger->critical(format, a1, a2, a3, a4, a5))
#define LOG_CRITICAL_6(logger, format, a1, a2, a3, a4, a5, a6) (logger->critical(format, a1, a2, a3, a4, a5, a6))
#define LOG_CRITICAL_7(logger, format, a1, a2, a3, a4, a5, a6, a7) (logger->critical(format, a1, a2, a3, a4, a5, a6, a7))
#define LOG_CRITICAL_8(logger, format, a1, a2, a3, a4, a5, a6, a7, a8) (logger->critical(format, a1, a2, a3, a4, a5, a6, a7, a8))
#define LOG_CRITICAL_9(logger, format, a1, a2, a3, a4, a5, a6, a7, a8, a9) (logger->critical(format, a1, a2, a3, a4, a5, a6, a7, a8, a9))

#endif
