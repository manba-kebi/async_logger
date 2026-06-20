#pragma once

#include <string>
#include <string_view>

namespace asynclogger {

	enum class LogLevel {
		Info,
		Warn,
		Error,
	};

	std::string_view to_string(LogLevel level) noexcept;	//把 LogLevel 转成字符串

}	