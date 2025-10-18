#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <unordered_map>

extern "C" {
#include <zlog.h>
}

#define LOGGER_DEBUG(cat, fmt, ...) loggerSpace::CategoryLogger::instance().debug(#cat, fmt, ##__VA_ARGS__)
#define LOGGER_INFO(cat, fmt, ...)  loggerSpace::CategoryLogger::instance().info(#cat, fmt, ##__VA_ARGS__)
#define LOGGER_NOTICE(cat, fmt, ...)  loggerSpace::CategoryLogger::instance().notice(#cat, fmt, ##__VA_ARGS__)
#define LOGGER_WARN(cat, fmt, ...)  loggerSpace::CategoryLogger::instance().warn(#cat, fmt, ##__VA_ARGS__)
#define LOGGER_ERROR(cat, fmt, ...) loggerSpace::CategoryLogger::instance().error(#cat, fmt, ##__VA_ARGS__)
#define LOGGER_FATAL(cat, fmt, ...) loggerSpace::CategoryLogger::instance().fatal(#cat, fmt, ##__VA_ARGS__)

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

    void logPrintf(const char* log);

    Logger();

    ~Logger();

private:
    bool initialized_;
    std::unordered_map<std::string, zlog_category_t*> cats_;

private:
    static constexpr const char* CONFIG_FILE = "/app/conf/zlog.conf";
    static constexpr const char* LOG_DIR = "/data/eyeLog";
};

class CategoryLogger {
public:
    static CategoryLogger &instance() {
        static CategoryLogger inst;
        return inst;
    }

    CategoryLogger(const CategoryLogger&) = delete;

    CategoryLogger &operator=(const CategoryLogger&) = delete;

    template <typename... Args>
    void debug(const char* category, const char* fmt, Args... args) {
        zlog_category_t* cat = Logger::instance().get_category(category);
        if (cat) zlog_debug(cat, fmt, args...);
    }

    template <typename... Args>
    void info(const char* category, const char* fmt, Args... args) {
        zlog_category_t* cat = Logger::instance().get_category(category);
        if (cat) zlog_info(cat, fmt, args...);
    }

    template <typename... Args>
    void notice(const char* category, const char* fmt, Args... args) {
        zlog_category_t* cat = Logger::instance().get_category(category);
        if (cat) zlog_notice(cat, fmt, args...);
    }

    template <typename... Args>
    void warn(const char* category, const char* fmt, Args... args) {
        zlog_category_t* cat = Logger::instance().get_category(category);
        if (cat) zlog_warn(cat, fmt, args...);
    }

    template <typename... Args>
    void error(const char* category, const char* fmt, Args... args) {
        zlog_category_t* cat = Logger::instance().get_category(category);
        if (cat) zlog_error(cat, fmt, args...);
    }

    template <typename... Args>
    void fatal(const char* category, const char* fmt, Args... args) {
        zlog_category_t* cat = Logger::instance().get_category(category);
        if (cat) zlog_fatal(cat, fmt, args...);
    }

private:

    CategoryLogger(){};

    ~CategoryLogger(){};
};

}

#endif  // LOGGER_H