#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>

extern "C" {
#include <zlog.h>
}

enum LogCategory {
    HIMPP,
    LOG_MAX
};

#define LOGGER_DEBUG(cat, fmt, ...)                                   \
    do {                                                              \
        zlog_category_t* __logger_cat_tmp =                           \
            loggerSpace::Logger::instance().get_category((#cat));     \
        if (__logger_cat_tmp)                                         \
            zlog_debug(__logger_cat_tmp, (fmt), ##__VA_ARGS__);       \
    } while (0)

#define LOGGER_INFO(cat, fmt, ...)                                    \
    do {                                                              \
        zlog_category_t* __logger_cat_tmp =                           \
            loggerSpace::Logger::instance().get_category((#cat));     \
        if (__logger_cat_tmp)                                         \
            zlog_info(__logger_cat_tmp, (fmt), ##__VA_ARGS__);        \
    } while (0)

#define LOGGER_NOTICE(cat, fmt, ...)                                  \
    do {                                                              \
        zlog_category_t* __logger_cat_tmp =                           \
            loggerSpace::Logger::instance().get_category((#cat));     \
        if (__logger_cat_tmp)                                         \
            zlog_notice(__logger_cat_tmp, (fmt), ##__VA_ARGS__);      \
    } while (0)

#define LOGGER_WARN(cat, fmt, ...)                                    \
    do {                                                              \
        zlog_category_t* __logger_cat_tmp =                           \
            loggerSpace::Logger::instance().get_category((#cat));     \
        if (__logger_cat_tmp)                                         \
            zlog_warn(__logger_cat_tmp, (fmt), ##__VA_ARGS__);        \
    } while (0)

#define LOGGER_ERROR(cat, fmt, ...)                                   \
    do {                                                              \
        zlog_category_t* __logger_cat_tmp =                           \
            loggerSpace::Logger::instance().get_category((#cat));     \
        if (__logger_cat_tmp)                                         \
            zlog_error(__logger_cat_tmp, (fmt), ##__VA_ARGS__);       \
    } while (0)

#define LOGGER_FATAL(cat, fmt, ...)                                   \
    do {                                                              \
        zlog_category_t* __logger_cat_tmp =                           \
            loggerSpace::Logger::instance().get_category((#cat));     \
        if (__logger_cat_tmp)                                         \
            zlog_fatal(__logger_cat_tmp, (fmt), ##__VA_ARGS__);       \
    } while (0)

namespace loggerSpace {

class Logger {
public:

    static Logger &instance() {
        static Logger inst;
        return inst;
    }

    Logger(const Logger&) = delete;

    Logger &operator=(const Logger&) = delete;

    bool init() noexcept;

    void fini() noexcept;

    bool initialized() const;

    zlog_category_t *get_category(const std::string &cat);

    zlog_category_t *get_category(const char * cat);

private:

    void log_printf(const char* log);

    Logger();

    ~Logger();

    int reload_log_config();

    int check_config_task();

private:
    bool initialized_;
    std::unordered_map<std::string, zlog_category_t*> cats_;
    std::thread check_config_thread_;
    std::atomic<bool> stop_flag_;

private:
    static constexpr const char* CONFIG_FILE = "/app/conf/zlog.conf";
    static constexpr const char* UPDATE_FILE = "/app/conf/logupdate";
    static constexpr const char* LOG_DIR = "/data/eyeLog";
};

}

#endif  // LOGGER_H