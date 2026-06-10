#include "asynclogger/async_logger.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
	std::string read_file(const std::filesystem::path& path) {
		std::ifstream input(path, std::ios::binary);
		std::ostringstream buffer;
		buffer << input.rdbuf();
		return buffer.str();
	}

	void reset_test_logs() {
		std::filesystem::remove_all("test_logs");
		std::filesystem::create_directories("test_logs");
	}
}

void test_basic_logging() {
	reset_test_logs();

	asynclogger::LoggerConfig config;
	config.file_path = "test_logs/basic.log";
	config.max_queue_size = 16;
	config.roll_size_bytes = 1024 * 1024;

	asynclogger::AsyncLogger logger(config);
	assert(logger.info("hello info"));
	assert(logger.warn("hello warn"));
	assert(logger.error("hello error"));
	logger.stop();

	const std::string content = read_file("test_logs/basic.log");
	assert(content.find("hello info") != std::string::npos);
	assert(content.find("hello warn") != std::string::npos);
	assert(content.find("hello error") != std::string::npos);
}

void test_multi_thread_logging() {
	reset_test_logs();

	asynclogger::LoggerConfig config;
	config.file_path = "test_logs/multi.log";
	config.max_queue_size = 1024;
	config.roll_size_bytes = 1024 * 1024;

	asynclogger::AsyncLogger logger(config);

	std::vector<std::thread> threads;
	for (int i = 0; i < 4; i++) {
		threads.emplace_back([i, &logger] {
			for (int n = 0; n < 200; ++n) {
				logger.info("thread-" + std::to_string(i) + "-message-" + std::to_string(n));
			}
			});
	}
	for (auto& thread : threads) {
		thread.join();
	}

	logger.stop();

	const std::string content = read_file("test_logs/multi.log");
	assert(content.find("thread-0-message-0") != std::string::npos);
	assert(content.find("thread-1-message-0") != std::string::npos);
	assert(content.find("thread-2-message-0") != std::string::npos);
	assert(content.find("thread-3-message-0") != std::string::npos);
}

void test_drop_policy_does_not_block_forever() {
	reset_test_logs();

	asynclogger::LoggerConfig config;
	config.file_path = "test_logs/drop.log";
	config.max_queue_size = 1;
	config.roll_size_bytes = 1024 * 1024;
	config.overflow_policy = asynclogger::OverflowPolicy::Drop;

	asynclogger::AsyncLogger logger(config);

	for (int i = 0; i < 1000; i++) {
		logger.info("drop-test-" + std::to_string(i));
	}
	
	logger.stop();
	assert(std::filesystem::exists("test_logs/drop.log"));
}

void test_log_rotation() {
	reset_test_logs();

	asynclogger::LoggerConfig config;
	config.file_path = "test_logs/roll.log";
	config.max_queue_size = 128;
	config.roll_size_bytes = 256;

	asynclogger::AsyncLogger logger(config);
	for (int i = 0; i < 40; i++) {
		logger.info("this is a long log line use to trigger file rolling " + std::to_string(i));
	}
	logger.stop();

	assert(std::filesystem::exists("test_logs/roll.log"));
	assert(std::filesystem::exists("test_logs/roll.log.1"));
}

int main() {
	test_basic_logging();
	test_multi_thread_logging();
	test_drop_policy_does_not_block_forever();
	test_log_rotation();
	return 0;
}