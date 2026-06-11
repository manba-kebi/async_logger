#include "asynclogger/async_logger.h"

int main() {
    asynclogger::LoggerConfig config;
    config.file_path = "logs/basic_usage.log";
    config.max_queue_size = 1024;
    config.roll_size_bytes = 10*1024*1024;
    config.overflow_policy = asynclogger::OverflowPolicy::Block;

    asynclogger::AsyncLogger logger(config);

    logger.info("server started");
    logger.warn("queue is nearly full");
    logger.error("bind failed");

    logger.flush();
    return 0;
}