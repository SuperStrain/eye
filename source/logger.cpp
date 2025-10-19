#include "logger.h"
#include <sys/stat.h>
#include <unistd.h>

void loggerSpace::Logger::log_printf(const char* log)
{
    printf("[zlog] %s\n", log);
}

loggerSpace::Logger::Logger() : initialized_(false)
{
    stop_flag_.store(true);
}

loggerSpace::Logger::~Logger()
{
    fini();
}

bool loggerSpace::Logger::init() noexcept
{
    if(access(LOG_DIR, F_OK) != 0) {
        mkdir(LOG_DIR, 0755);
    }

    if(initialized_)
    {
        log_printf("Logger has initialized");
        return false;
    }

    if(zlog_init(CONFIG_FILE) != 0)
    {
        log_printf("Logger initialize fail!");
        initialized_ = false;
        return false;        
    }
    initialized_ = true;

    stop_flag_.store(false);
    check_config_thread_ = std::thread(&loggerSpace::Logger::check_config_task, this);

    char logBuff[128] = {0};
    snprintf(logBuff, sizeof(logBuff), "Logger initialize success! zlog version : %s", zlog_version());
    log_printf(logBuff);

    return true;
}

void loggerSpace::Logger::fini() noexcept
{
    if(!initialized_)
    {
        log_printf("Logger fini");
        return ;
    }

    zlog_fini();
    initialized_ = false;

    stop_flag_.store(true);
    if(check_config_thread_.joinable())
        check_config_thread_.join();
}

bool loggerSpace::Logger::initialized() const
{
    return initialized_;
}


zlog_category_t * loggerSpace::Logger::get_category(const std::string &cat) {
    if (!initialized_)
    {
        log_printf("Logger not initialize");
        return nullptr;
    }
    auto it = cats_.find(cat);
    if (it != cats_.end()) return it->second;
    zlog_category_t *zc = zlog_get_category(cat.c_str());
    if (zc) cats_.emplace(cat, zc);
    return zc;
}

zlog_category_t * loggerSpace::Logger::get_category(const char * cat) {
    if (!initialized_)
    {
        log_printf("Logger not initialize");
        return nullptr;
    }
    if(!cat)
    {
        log_printf("cat is null");
        return nullptr;        
    }

    auto it = cats_.find(cat);
    if (it != cats_.end()) return it->second;
    zlog_category_t *zc = zlog_get_category(cat);
    if (zc) cats_.emplace(cat, zc);
    return zc;
}

int loggerSpace::Logger::reload_log_config()
{
    if (zlog_reload(CONFIG_FILE) != 0)
    {
        log_printf("log config reload fail!");
        return -1;
    }
    return 0;
}

int loggerSpace::Logger::check_config_task()
{
    while(!stop_flag_.load())
    {
        if(access(UPDATE_FILE, F_OK) == 0)
        {
            reload_log_config();
            remove(UPDATE_FILE);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}