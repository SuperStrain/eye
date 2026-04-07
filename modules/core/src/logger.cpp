#include "logger.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>

const char* LogCategoryNames[] = {
#define X(name) #name,
    LOG_CATEGORY_LIST
#undef X
};

constexpr std::chrono::seconds loggerSpace::Logger::CHECK_INTERVAL;

void loggerSpace::Logger::log_printf(const char* log)
{
    printf("[ZLOG] %s\n", log);
}

loggerSpace::Logger::Logger() : initialized_(false)
{
    stop_flag_.store(true);
}

loggerSpace::Logger::~Logger()
{
    fini();
}

bool loggerSpace::Logger::ensure_dir(const char* path)
{
    if (access(path, F_OK) == 0)
        return true;

    size_t len = strlen(path);
    char tmp[256] = {0};
    if (len >= sizeof(tmp))
        return false;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (access(tmp, F_OK) != 0) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                    return false;
            }
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return false;

    return true;
}

void loggerSpace::Logger::preload_categories()
{
    cats_.clear();
    for (int i = 0; i < LOG_MAX; ++i) {
        const char* name = LogCategoryNames[i];
        zlog_category_t* zc = zlog_get_category(name);
        if (zc) {
            cats_.emplace(name, zc);
        } else {
            char buf[128] = {0};
            snprintf(buf, sizeof(buf), "preload category [%s] fail", name);
            log_printf(buf);
        }
    }
}

bool loggerSpace::Logger::init() noexcept
{
    if (!ensure_dir(LOG_DIR)) {
        char buf[128] = {0};
        snprintf(buf, sizeof(buf), "create log dir [%s] fail, errno=%d", LOG_DIR, errno);
        log_printf(buf);
    }

    if (initialized_)
    {
        log_printf("Logger already initialized");
        return true;
    }

    if (zlog_init(CONFIG_FILE) != 0)
    {
        log_printf("Logger initialize fail!");
        return false;
    }

    initialized_ = true;
    preload_categories();

    stop_flag_.store(false);
    check_config_thread_ = std::thread(&loggerSpace::Logger::check_config_task, this);

    char logBuff[128] = {0};
    snprintf(logBuff, sizeof(logBuff), "Logger initialize success! zlog version : %s", zlog_version());
    log_printf(logBuff);

    return true;
}

void loggerSpace::Logger::fini() noexcept
{
    if (!initialized_)
    {
        return;
    }

    stop_flag_.store(true);
    if (check_config_thread_.joinable())
        check_config_thread_.join();

    zlog_fini();
    initialized_ = false;
    cats_.clear();
}

bool loggerSpace::Logger::initialized() const
{
    return initialized_;
}

zlog_category_t * loggerSpace::Logger::get_category(const std::string &cat) {
    if (!initialized_)
    {
        return nullptr;
    }
    auto it = cats_.find(cat);
    if (it != cats_.end()) return it->second;
    return nullptr;
}

zlog_category_t * loggerSpace::Logger::get_category(const char * cat) {
    if (!initialized_)
    {
        return nullptr;
    }
    if (!cat)
    {
        return nullptr;
    }

    auto it = cats_.find(cat);
    if (it != cats_.end()) return it->second;
    return nullptr;
}

int loggerSpace::Logger::reload_log_config()
{
    if (zlog_reload(CONFIG_FILE) != 0)
    {
        log_printf("log config reload fail!");
        return -1;
    }
    preload_categories();
    return 0;
}

int loggerSpace::Logger::check_config_task()
{
    while (!stop_flag_.load())
    {
        if (access(UPDATE_FILE, F_OK) == 0)
        {
            int ret = reload_log_config();
            if (ret == 0)
            {
                log_printf("update log config success!");
            }
            remove(UPDATE_FILE);
        }
        std::this_thread::sleep_for(CHECK_INTERVAL);
    }
    return 0;
}
