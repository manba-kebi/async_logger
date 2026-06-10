#include "asynclogger/async_logger.h"

#include <utility>

namespace asnclogger {
	AsyncLogger::AsyncLogger(LoggerConfig config)
		: config_(std::move(config)),
		queue_(config_.max_queue_size),
		file_(config_.file_path,config_.roll_size_bytes),
		worker_([this] { worker_loop(); }){ }
	
	AsyncLogger::~AsyncLogger() {
		stop();
	}

	bool AsyncLogger::log(LogLevel level, std::string message) {
		if (stopped_.load()) {
			return false;
		}

		LogMessage log_message;
		log_message.level = level;
		log_message.text = std::move(message);
		log_message.timestamp = std::chrono::system_clock::now();
		log_message.thread_id = std::this_thread::get_id();

		increment_pending();

		bool accepted = false;
		if (config_.overflow_policy == OverflowPolicy::Block) {
			accepted = queue_.push(std::move(log_message));
		}
		else {
			accepted = queue_.try_push(std::move(log_message));
		}

		if (!accepted) {
			cancel_onw_message();
			dropped_count_.fetch_add(1);
			return false;
		}

		return true;
	}
	bool AsyncLogger::info(std::string message);
	bool AsyncLogger::warn(std::string message);
	bool AsyncLogger::error(std::string message);

	void AsyncLogger::flush();
	void AsyncLogger::stop();

	std::uint64_t AsyncLogger::deopped_count() const noexcept;
}