#pragma once

#include <iostream>
#include <string>
#include <string_view>

namespace asynclogger {

	enum class LogLevel {
		Info,
		Warn,
		Error,
	};

	std::string_view to_string(LogLevel level) noexcept;	//°Ñ LogLevel ×ª³É×Ö·û´®

}	