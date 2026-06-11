#include "asynclogger/async_logger.h"

#include <utility>

namespace asynclogger {
	AsyncLogger::AsyncLogger(LoggerConfig config)
		: config_(std::move(config)),
		queue_(config_.max_queue_size),
		file_(config_.file_path,config_.roll_size_bytes),
		worker_([this] { worker_loop(); }){ }
	
	AsyncLogger::~AsyncLogger() {
		stop();
	}

	bool AsyncLogger::log(LogLevel level, std::string message) {
		if (stopped_.load()) {		// load()用来原子地读取当前值。
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
			//绝对不能丢日志的场景（如金融交易、安全审计）→ 用 Block，但要注意：如果磁盘写入慢，大量线程可能阻塞，甚至拖垮系统。
			accepted = queue_.push(std::move(log_message));
		}
		else {
			//高吞吐、可容忍丢弃的场景（如微服务请求追踪、调试日志）→ 用 Drop，保证生产线程永不阻塞。
			accepted = queue_.try_push(std::move(log_message));
		}

		if (!accepted) {
			cancel_one_message();
			dropped_count_.fetch_add(1);	//fetch_add(1) 原子地将它的值加 1，并返回旧值（这里没用返回值）。
			return false;
		}

		return true;
	}

	bool AsyncLogger::info(std::string message) {
		return log(LogLevel::Info,std::move(message));
	}

	bool AsyncLogger::warn(std::string message) {
		return log(LogLevel::Warn,std::move(message));
	}

	bool AsyncLogger::error(std::string message) {
		return log(LogLevel::Error,std::move(message));
	}

	//flush() 确保“所有已提交的日志都写入文件了”
	//将缓冲的数据强制刷到磁盘
	void AsyncLogger::flush() {
		//之前的 log() 会递增 pending_count_（待处理消息数）。
		//后台线程每写完一条，调用 finish_one_message 递减 pending_count_ 并 notify_all()。
		{
			std::unique_lock<std::mutex> lock(pending_mutex_);
			pending_cv_.wait(lock,[this] {		//等待所有待处理消息处理完，等待直到 pending_count_ 变成 0。
				return pending_count_ == 0;
				//flush() 在这里等待 pending_count_ == 0，意味着队列里和正在写的那一条全部完成
			});
		}

		std::lock_guard<std::mutex> lock(file_mutex_);
		file_.flush();
		//此时再真正 file_.flush()，保证所有内容落盘。
	}

	void AsyncLogger::stop() {
		const bool already_stopped = stopped_.exchange(true);
		if (already_stopped) {
			return ;
		}

		queue_.close();

		if (worker_.joinable()) {
			worker_.join();
		}

		std::lock_guard<std::mutex> lock(file_mutex_);
		file_.flush();
	}

	std::uint64_t AsyncLogger::deopped_count() const noexcept {
		return dropped_count_.load();
	}

	void AsyncLogger::worker_loop() {
		LogMessage message;
		while (queue_.pop(message)) {
			const std::string line = format_log_message(message);

			{
				std::lock_guard<std::mutex> lock(file_mutex_);
				file_.write(line);
				if (config_.auto_flush) {
					file_.flush();
				}
			}

			finish_one_message();
		}

		std::lock_guard<std::mutex> lock(file_mutex_);
		file_.flush();
	}

	void AsyncLogger::increment_pending() {
		std::lock_guard<std::mutex> lock(pending_mutex_);
		++pending_count_;
	}

	//正常处理完一条消息，完成计数。
	void AsyncLogger::finish_one_message() {
		{
			std::lock_guard<std::mutex> lock(file_mutex_);
			if (pending_count_ > 0) {
				--pending_count_;
			}
		}
		pending_cv_.notify_all();
	}

	//因为异常或丢弃而取消一条消息，取消计数。
	void AsyncLogger::cancel_one_message() {
		finish_one_message();
	}
}