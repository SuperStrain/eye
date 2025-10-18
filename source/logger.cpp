#include "logger.h"
#include <sys/stat.h>
#include <unistd.h>

void loggerSpace::Logger::logPrintf(const char* log)
{
    printf("[zlog] %s\n", log);
}

loggerSpace::Logger::Logger() : initialized_(false)
{

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
        logPrintf("Logger has initialized");
        return false;
    }

    if(zlog_init(CONFIG_FILE) != 0)
    {
        logPrintf("Logger initialize fail!");
        initialized_ = false;
        return false;        
    }
    initialized_ = true;
    char logBuff[128] = {0};
    snprintf(logBuff, sizeof(logBuff), "Logger initialize success! zlog version : %s", zlog_version());
    logPrintf(logBuff);

    return true;
}

void loggerSpace::Logger::fini() noexcept
{
    if(!initialized_)
    {
        logPrintf("Logger has fini");
        return ;
    }

    zlog_fini();
    initialized_ = false;
}

bool loggerSpace::Logger::initialized() const
{
    return initialized_;
}


zlog_category_t * loggerSpace::Logger::get_category(const std::string &cat) {
    if (!initialized_)
    {
        logPrintf("Logger not initialize");
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
        logPrintf("Logger not initialize");
        return nullptr;
    }
    if(!cat)
    {
        logPrintf("cat is null");
        return nullptr;        
    }

    auto it = cats_.find(cat);
    if (it != cats_.end()) return it->second;
    zlog_category_t *zc = zlog_get_category(cat);
    if (zc) cats_.emplace(cat, zc);
    return zc;
}