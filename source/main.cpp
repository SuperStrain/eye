#include <iostream>
#include <vector>
#include <csignal>
#include <unistd.h>
#include "logger.h"
#include "video_process_hi.h"

void signal_handler(int sig) {
    std::cout << "Signal " << sig << " received" << std::endl;
}

int main() {
    // 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);

    loggerSpace::Logger::instance().init();

    videoProcessHi::getInstance().init();

    pause();

    loggerSpace::Logger::instance().fini();

    std::cout << "eye exiting..." << std::endl;
    sleep(1);
    std::cout << "eye exiting gracefully." << std::endl;
    return 0;
}