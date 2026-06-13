#include "asynclogger/log_message.h"

#include <cassert>
#include <string>

int main() {
	assert(asynclogger::to_string(asynclogger::LogLevel::Info) == "INFO");
	assert(asynclogger::to_string(asynclogger::LogLevel::Warn) == "WARN");
	assert(asynclogger::to_string(asynclogger::LogLevel::Error) == "ERROR");

	asynclogger::LogMessage message;
	message.level = asynclogger::LogLevel::Info;
	message.text = "formatter works";

	const std::string line = asynclogger::format_log_message(message);

	assert(line.find("INFO") != std::string::npos);
	assert(line.find("formatter works") != std::string::npos);
	assert(!line.empty());
	assert(line.back() == '\n');

	int failures = 0;
	failures += test::run("test name", test_function);
	failures += test::run("another test", another_test_function);
	return failures == 0 ? 0 : 1;
}