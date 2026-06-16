#include "asynclogger/log_message.h"

#include <chrono>
#include <regex>		//用来描述一个字符匹配模式
#include <string>
#include <thread>

#include "test_support.h"

namespace {

	void test_level_names() {
		test::require(asynclogger::to_string(asynclogger::LogLevel::Info)=="INFO","Info must format as INFO");
		test::require(asynclogger::to_string(asynclogger::LogLevel::Warn)=="WARN","Warn must format as WARN");
		test::require(asynclogger::to_string(asynclogger::LogLevel::Error)=="ERROR","Error must format as ERROR");
	}

	void test_full_line_shape() {
		asynclogger::LogMessage message;
		message.level = asynclogger::LogLevel::Info;
		message.text = "formatter works";
		message.timestamp = std::chrono::system_clock::now();
		message.thread_id = std::this_thread::get_id();

		const std::string line = asynclogger::format_log_message(message);

		//正则表达式
		const std::regex expected(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} \[INFO\] \[tid=.+\] formatter works\n$)");

		test::require(std::regex_match(line,expected),"formatted line does not match the expected shape");
	}

	void test_each_message_ends_with_one_newline() {
		asynclogger::LogMessage message;
		message.level = asynclogger::LogLevel::Warn;
		message.text = "single line";

		const std::string line = asynclogger::format_log_message(message);

		test::require(!line.empty(),"formatted line must not be empty");
		test::require(line.back() == '\n',"formatted line must end with a newline");
		test::require_equal(test::count_lines(line),std::size_t{1},"one message must produce exactly one line");
	}

	void test_embedded_line_breaks_are_escaped() {
		asynclogger::LogMessage message;
		message.level = asynclogger::LogLevel::Error;
		message.text = "first\nsecond\rthird";

		const std::string line = asynclogger::format_log_message(message);

		test::require(line.find("first\\nsecond\\rthird") != std::string::npos,"embedded line breaks must be escaped");	//嵌入换行符必须进行转义
		test::require_equal(test::count_lines(line),std::size_t{1},"escaped message must remain one physical line");	//转义消息必须保持在一行内
	}

}	//namespace

int main() {
	int failures = 0;

	failures += test::run("level names",test_level_names);
	failures += test::run("full line shape",test_full_line_shape);
	failures += test::run("one message ends with one newline",test_each_message_ends_with_one_newline);
	failures += test::run("embedded line breaks are escaped",test_embedded_line_breaks_are_escaped);

	return failures == 0?0:1;
}
