#include "logger.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/syslog_sink.h"

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

#define DEFAULT_MAX_LOG_FILE_SIZE 0x1000000
#define DEFAULT_MAX_LOG_FILE_COUNT 8

#define DEFAULT_STDOUT_LEVEL UBIQUITY_MAX_LOG_INFO
#define DEFAULT_STDERR_LEVEL UBIQUITY_MIN_LOG_WARNING
#define DEFAULT_SYSLOG_LEVEL UBIQUITY_LOG_OFF

#define DEFAULT_LOG_PATTERN ("[%^%L%$] time=%Y/%m/%d-%H:%M:%S.%f%z %v")

template<typename Mutex>
class filtered_sink_t : public spdlog::sinks::base_sink<Mutex> {
    ubiquity_log_level_t level;
    bool is_rotating_sink = false;
    bool is_daily_sink = false;
    int rotation_hour;
    int rotation_minute;
    uint16_t max_archives = DEFAULT_MAX_LOG_FILE_COUNT;
    size_t max_size = DEFAULT_MAX_LOG_FILE_SIZE;
    std::shared_ptr<spdlog::sinks::sink> downstream_sink;

public:
    filtered_sink_t(
            ubiquity_log_level_t level
            ) : level(level)
    {
    }

    void set_filter_level(
            ubiquity_log_level_t level
            )
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->level = level;
    }

    void make_stdout_sink(void)
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->downstream_sink.reset();
        this->downstream_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        this->downstream_sink->set_pattern(DEFAULT_LOG_PATTERN);
    }

    void make_stderr_sink(void)
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->downstream_sink.reset();
        this->downstream_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        this->downstream_sink->set_pattern(DEFAULT_LOG_PATTERN);
    }

    void make_syslog_sink(void)
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->downstream_sink.reset();
        this->downstream_sink = std::make_shared<spdlog::sinks::syslog_sink_mt>("ubiquity", LOG_PID, LOG_USER, true);
        this->downstream_sink->set_pattern(DEFAULT_LOG_PATTERN);
    }

    void make_basic_file_sink(
            const char *file_path
            )
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->downstream_sink.reset();
        this->downstream_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path);
        this->downstream_sink->set_pattern(DEFAULT_LOG_PATTERN);
        this->is_rotating_sink = false;
        this->is_daily_sink = false;
    }

    void make_rotating_file_sink(
            const char *file_path
            )
    {
        this->downstream_sink.reset();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, this->max_size, this->max_archives);
        file_sink->set_pattern(DEFAULT_LOG_PATTERN);
        this->downstream_sink = file_sink;
        this->is_rotating_sink = true;
        this->is_daily_sink = false;
    }

    void make_daily_file_sink(
            const char *file_path
            )
    {
        this->downstream_sink.reset();
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                file_path,
                this->rotation_hour,
                this->rotation_minute,
                false,
                this->max_archives);
        file_sink->set_pattern(DEFAULT_LOG_PATTERN);
        this->downstream_sink = file_sink;
        this->is_rotating_sink = false;
        this->is_daily_sink = true;
    }

    void set_rotation_time(
            const char *file_path,
            const int rotation_hour,
            const int rotation_minute
            )
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->rotation_hour = rotation_hour;
        this->rotation_minute = rotation_minute;
        this->make_daily_file_sink(file_path);
    }

    void set_max_size(
            const char *file_path,
            const size_t max_size
            )
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->max_size = max_size;
        this->make_rotating_file_sink(file_path);
    }

    void set_max_archives(
            const char *file_path,
            const size_t max_archives
            )
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        this->max_archives = max_archives;
        if (this->is_daily_sink) {
            this->make_daily_file_sink(file_path);
        } else {
            this->make_rotating_file_sink(file_path);
        }
    }

protected:
    void sink_it_(
            const spdlog::details::log_msg& message
            ) override
    {
        if (get_ubiquity_log_level(message.level) & this->level) {
            this->downstream_sink->log(message);
        }
    }

    void flush_(void) override {
        this->downstream_sink->flush();
    }
};

using filtered_sink_mt = filtered_sink_t<std::mutex>;

static ubiquity_log_level_t s_log_level = UBIQUITY_MIN_LOG_INFO;

std::vector<spdlog::sink_ptr> s_sinks{};

static std::shared_ptr<filtered_sink_mt> s_stdout_sink;
static std::shared_ptr<filtered_sink_mt> s_stderr_sink;
static std::shared_ptr<filtered_sink_mt> s_syslog_sink;

static std::unordered_map<std::string, std::shared_ptr<filtered_sink_mt>> s_file_sinks;

static inline void initialize_if_necessary(
        ) noexcept
{
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> guard(s_mutex);

    static bool s_initialized = false;
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    // Create sinks for stdout, stderr and syslog.
    s_stdout_sink = std::make_shared<filtered_sink_mt>(DEFAULT_STDOUT_LEVEL);
    s_stdout_sink->make_stdout_sink();
    s_stderr_sink = std::make_shared<filtered_sink_mt>(DEFAULT_STDERR_LEVEL);
    s_stderr_sink->make_stderr_sink();
    s_syslog_sink = std::make_shared<filtered_sink_mt>(DEFAULT_SYSLOG_LEVEL);
    s_syslog_sink->make_syslog_sink();

    // Add sinks to the sink vector.
    s_sinks.push_back(s_stdout_sink);
    s_sinks.push_back(s_stderr_sink);
    s_sinks.push_back(s_syslog_sink);
}

void ubiquity_logger_set_default_level(
        const ubiquity_log_level_t log_level
        ) noexcept
{
    s_log_level = log_level;
}

static inline std::shared_ptr<filtered_sink_mt> ubiquity_logger_get_file_sink(
        const char *const file_path
        ) noexcept
{
    auto it = s_file_sinks.find(std::string(file_path));
    if (s_file_sinks.end() == it) {
        auto file_sink = std::make_shared<filtered_sink_mt>(s_log_level);
        file_sink->make_basic_file_sink(file_path);
        s_file_sinks[file_path] = file_sink;
        s_sinks.push_back(file_sink);
        return file_sink;
    }
    return it->second;
}

void ubiquity_logger_set_file_sink_level(
        const char *const file_path,
        const ubiquity_log_level_t level
        ) noexcept
{
    initialize_if_necessary();
    auto file_sink = ubiquity_logger_get_file_sink(file_path);
    file_sink->set_filter_level(level);
}

void ubiquity_logger_set_file_sink_daily_rotation(
        const char *const file_path,
        const int rotation_hour,
        const int rotation_minute
        ) noexcept
{
    initialize_if_necessary();
    auto file_sink = ubiquity_logger_get_file_sink(file_path);
    file_sink->set_rotation_time(file_path, rotation_hour, rotation_minute);
}

void ubiquity_logger_set_file_sink_max_size(
        const char *const file_path,
        const size_t max_file_size
        ) noexcept
{
    initialize_if_necessary();
    auto file_sink = ubiquity_logger_get_file_sink(file_path);
    file_sink->set_max_size(file_path, max_file_size);
}

void ubiquity_logger_set_file_sink_max_archives(
        const char *const file_path,
        const size_t max_archives
        ) noexcept
{
    initialize_if_necessary();
    auto file_sink = ubiquity_logger_get_file_sink(file_path);
    file_sink->set_max_archives(file_path, max_archives);
}

void ubiquity_logger_set_stdout_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    initialize_if_necessary();
    s_stdout_sink->set_filter_level(level);
}

void ubiquity_logger_set_stderr_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    initialize_if_necessary();
    s_stderr_sink->set_filter_level(level);
}

void ubiquity_logger_set_syslog_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    initialize_if_necessary();
    s_syslog_sink->set_filter_level(level);
}

static ubiquity_log_level_t s_protocol_log_level = UBIQUITY_LOG_NONE;

void ubiquity_logger_set_protocol_log_level(
        const ubiquity_log_level_t level
        ) noexcept
{
    s_protocol_log_level = level;
}

static ubiquity_logger_t s_protocol_logger = NULL;
static size_t s_protocol_logger_use_count = 0;

ubiquity_logger_t ubiquity_logger_create(
        const bool is_protocol
        ) noexcept
{
    static size_t unique_id = 1;

    initialize_if_necessary();
    if (is_protocol) {
        if (NULL == s_protocol_logger) {
            s_protocol_logger = std::make_shared<filtered_logger_t>(
                    std::to_string(unique_id++),
                    s_protocol_log_level,
                    s_sinks
                    );
        }
        ++s_protocol_logger_use_count;
        return s_protocol_logger;
    }
    return std::make_shared<filtered_logger_t>(
            std::to_string(unique_id++),
            s_log_level,
            s_sinks
            );
}

void ubiquity_logger_delete(
        ubiquity_logger_t logger
        ) noexcept
{
    if (logger == s_protocol_logger) {
        if (NULL != s_protocol_logger && 0 < s_protocol_logger_use_count) {
            --s_protocol_logger_use_count;
            if (0 == s_protocol_logger_use_count) {
                s_protocol_logger->flush();
                s_protocol_logger.reset();  // release shared_ptr → logger destroyed here,
                                            // while s_sinks is still alive
            }
        }
        return;
    }
    if (logger) {
        logger->flush();
        logger.reset();   // for non-protocol loggers, just release the shared_ptr
                          // caller's copy goes out of scope naturally, but
                          // explicit flush here ensures no lost tail logs
    }
}

void ubiquity_logger_set_level(
        ubiquity_logger_t logger,
        const ubiquity_log_level_t level
        ) noexcept
{
    logger->set_filter_level(level);
}
