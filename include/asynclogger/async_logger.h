#pragma once

#include <iostream>
#include <filesystem>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "asynclogger/blocking_queue.h"
#include "asynclogger/log_file.h"
#include "asynclogger/log_message.h"

using namespace std;

namespace asynclogger {
	enum class OverflowPolicy {
		Block,
		Drop,
	};
	struct LoggerConfig {
		std::filesystem::path file_path{"logs/app.log"};
		size_t max_queue_size = 1024;
		size_t roll_size_bytes = 10 * 1024 * 1024;
		OverflowPolicy overflow_policy{ OverflowPolicy::Block };
		bool auto_flush{ false };
	};

	class AsyncLogger {
	private:
		void worker_loop();
		void increment_pending();
		void finish_one_message();
		void cancel_one_message();
		
		LoggerConfig config_;
		BlockingQueue<LogMessage> queue_;
		LogFile file_;
		std::thread worker_;
		std::atomic<bool> stopped_{ false };
		std::atomic<std::uint64_t> dropped_count_{ 0 };

		std::mutex pending_mutex_;
		std::condition_variable pending_cv_;
		std::size_t pending_count_{ 0 };	//它是已提交但尚未完成写入的日志消息数量。
		//flush() 就利用它来判断是否所有待处理日志都已写完。

		std::mutex file_mutex_;
	public:
		explicit AsyncLogger(LoggerConfig config);
		~AsyncLogger();

		AsyncLogger(const AsyncLogger&) = delete;
		AsyncLogger& operator=(const AsyncLogger&) = delete;

		bool log(LogLevel level, std::string message);
		bool info(std::string message);
		bool warn(std::string message);
		bool error(std::string message);

		void flush();
		void stop();

		std::uint64_t deopped_count() const noexcept;
	};
}