#pragma once

#include "asynclogger/log_level.h"
#include <string>
#include <chrono>
#include <thread>


namespace asynclogger {
	struct LogMessage {
		LogLevel level{LogLevel::Info};					//日志级别
		std::string text;								//日志文本
		std::chrono::system_clock::time_point timestamp{ std::chrono::system_clock::now() };		//时间点
		std::thread::id thread_id{ std::this_thread::get_id() };									//线程 id

	};

	//把 `LogMessage` 变成一行文本
	//输出要做到：一条日志一行		包含时间、级别、线程信息、原始信息、以换行结尾
	std::string format_log_message(const LogMessage& message);
}