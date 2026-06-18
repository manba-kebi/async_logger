#include "asynclogger/async_logger.h"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "test_support.h"

namespace {

	const std::filesystem::path kRoot = std::filesystem::path("test_output") / "async_logger";

	asynclogger::LoggerConfig make_config(const std::filesystem::path& file_path) {
		asynclogger::LoggerConfig config;
		config.file_path = file_path;
		config.max_queue_size = 64;
		config.roll_size_bytes = 1024 * 1024;
		config.overflow_policy = asynclogger::OverflowPolicy::Block;
		config.auto_flush = false;
		return config;
	}

	//测试将三个不同日志等级的消息写入日志，能否正常写入
	void test_basic_logging_and_flush() {
		const auto directory = kRoot/"basic";
		test::reset_directory(directory);

		const auto path = directory / "app.log";
		auto config = make_config(path);

		asynclogger::AsyncLogger logger(config);

		test::require(logger.info("hello info"),"info log must be accepted");
		test::require(logger.warn("hello warn"),"warn log must be accepted");
		test::require(logger.error("hello error"),"error log must be accepted");

		logger.flush();

		const std::string content = test::read_text_file(path);

		test::require(content.find("[INFO]")!=std::string::npos,"INFO level must be written");
		test::require(content.find("[WARN]")!=std::string::npos,"WARN level must be written");
		test::require(content.find("[ERROR]")!=std::string::npos,"ERROR level must be written");
		test::require(content.find("hello info") != std::string::npos,"info message must be written");
		test::require(content.find("hello warn") != std::string::npos,"warn message must be written");
		test::require(content.find("hello error") != std::string::npos,"error message must be written");
		test::require_equal(test::count_lines(content),std::size_t{3},"three accepted messages must produce three lines");

		logger.stop();
	}

	//测试在Block模式下， AsyncLogger类对象析构时，是否会将close前的所有的消息全都消费掉
	void test_destructor_drains_pending_messages() {
		const auto directory = kRoot/"destructor";
		test::reset_directory(directory);

		const auto path = directory / "app.log";
		auto config = make_config(path);

		{
			asynclogger::AsyncLogger logger(config);
			for (int index = 0; index < 100; index++) {
				test::require(logger.info("message-" + std::to_string(index)),"block mode log must be accepted");
			}
		}

		const std::string content = test::read_text_file(path);
		test::require_equal(test::count_lines(content),std::size_t{100},"destructor must drain all accepted messages");
	}

	//测试关闭 logger 前后记录日志，看关闭前能否正常记录日志，关闭后是否禁止记录日志
	void test_stop_is_idempotent_and_rejects_new_logs() {
		const auto directory = kRoot/"stop";
		test::reset_directory(directory);

		const auto path = directory / "app.log";
		auto config = make_config(path);

		asynclogger::AsyncLogger logger(config);
		test::require(logger.info("before stop"),"pre-stop log must succeed");

		logger.stop();
		logger.stop();

		test::require(!logger.info("after stop"),"logger must reject messages after stop");

		const std::string content = test::read_text_file(path);
		test::require(content.find("before stop")!=std::string::npos,"pre-stop message must be persisted");
		test::require(content.find("after stop") == std::string::npos,"post-stop message must not be persisted");
	}

	//测试在阻塞模式下能否将多线程的生产的日志全都记录下来
	void test_block_mode_preserves_all_multithreaded_logs() {
		constexpr int kThreadCount = 4;				//constexpr 声明函数/变量能在编译期求值
		constexpr int kMessagesPerThread = 200;
		constexpr int kExpected = kThreadCount * kMessagesPerThread;

		const auto directory = kRoot/"multithread";
		test::reset_directory(directory);

		const auto path = directory / "app.log";
		auto config = make_config(path);
		config.max_queue_size = 32;
		config.overflow_policy = asynclogger::OverflowPolicy::Block;

		asynclogger::AsyncLogger logger(config);
		std::atomic<int> accepted_count{0};

		std::vector<std::thread> producers;
		producers.reserve(kThreadCount);

		for (int thread_index = 0; thread_index < kThreadCount; thread_index++) {
			producers.emplace_back([&,thread_index] {
				for (int message_index = 0; message_index < kMessagesPerThread; message_index++) {
					const std::string message = "thread-" + std::to_string(thread_index) + "-message-" +std::to_string(message_index);
					if (logger.info(message)) {
						accepted_count.fetch_add(1);
					}
				}
			});
		}

		for (auto& producer : producers) {
			producer.join();
		}

		logger.flush();
		logger.stop();

		test::require_equal(accepted_count.load(),kExpected,"Block mode must accept every message before stop");
		test::require_equal(logger.dropped_count(),uint64_t{0},"Block mode must not drop messages in this test");

		const std::string content = test::read_text_file(path);
		test::require_equal(test::count_lines(content),static_cast<std::size_t>(kExpected),"file line conut must equal produced message count");
	}

	//测试在Drop模式下，日志记录时，队列已满，是否真的会丢掉该日志
	void test_drop_mode_accounting_is_consistent() {
		constexpr int kAttemptCount = 5000;

		const auto directory = kRoot/"drop";
		test::reset_directory(directory);

		const auto path  = directory / "app.log";
		auto config = make_config(path);
		config.max_queue_size = 1;
		config.overflow_policy = asynclogger::OverflowPolicy::Drop;

		asynclogger::AsyncLogger logger(config);
		int accepted_count = 0;

		for (int index = 0; index < kAttemptCount; index++) {
			if (logger.info("drop-message-" + std::to_string(index))) {
				++accepted_count;
			}
		}

		logger.stop();

		const auto dropped_count = logger.dropped_count();

		test::require(accepted_count>0,"Drop mode must accept at least one message");
		test::require_equal(static_cast<std::uint64_t>(kAttemptCount),static_cast<std::uint64_t>(accepted_count)+dropped_count,"accepted and dropped counts must cover all attempts");

		const std::string content = test::read_text_file(path);
		test::require_equal(test::count_lines(content),static_cast<std::uint64_t>(accepted_count),"persisted line count must equal accepted count");
	}

	//测试写的字节数超过滚动阈值后，是否会生成新文件
	void test_async_logger_triggers_rotation() {
		const auto directory = kRoot/"rotation";
		test::reset_directory(directory);

		const auto path = directory / "app.log";
		auto config = make_config(path);
		config.roll_size_bytes = 256;

		asynclogger::AsyncLogger logger(config);

		for (int index = 0;index<40;++index) {
			test::require(logger.info("this is a long message used to trigger rotation" + std::to_string(index)),"rotation test log must be accepted");
		}

		logger.stop();

		test::require(std::filesystem::exists(path),"base log file must exist");
		test::require(std::filesystem::exists(path.string()+".1"),"at least one rolled log file must exist");
	}

	void test_background_write_failure_is_counted() {
		const auto directory = kRoot/"write_failure";
		test::reset_directory(directory);

		auto config = make_config(directory);		//这里把 `file_path` 设置成了一个已经存在的目录
		config.max_queue_size = 8;

		asynclogger::AsyncLogger logger(config);

		test::require(logger.info("this write will fail"),"message should be accepted into the queue");
		//`std::ofstream` 不能把目录当普通文件打开，所以后台写入会失败。

		logger.flush();
		logger.stop();

		test::require_equal(logger.dropped_count(),std::uint64_t{1},"background write failure must be counted as dropped");
	}
}	//namespace

int main() {
	int failures = 0;

	failures += test::run("basic logging and flush",test_basic_logging_and_flush);
	failures += test::run("destructor drains pending messages",test_destructor_drains_pending_messages);
	failures += test::run("stop is idempotent and rejects new logs",test_stop_is_idempotent_and_rejects_new_logs);
	failures += test::run("Block mode preservers multithreaded logs",test_block_mode_preserves_all_multithreaded_logs);
	failures += test::run("Drop mode accounting is consistent",test_drop_mode_accounting_is_consistent);
	failures += test::run("AsyncLogger triggers rotation",test_async_logger_triggers_rotation);
	failures += test::run("background write failure is counted",test_background_write_failure_is_counted);

	return failures == 0? 0:1;
}