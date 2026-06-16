#include "asynclogger/log_level.h"

namespace asynclogger {
	////把 LogLevel 转成字符串
	std::string_view to_string(LogLevel level) noexcept {
		switch (level) {
		case LogLevel::Info:
			return "INFO";
		case LogLevel::Warn:
			return "WARN";
		case LogLevel::Error:
			return "ERROR";
		}

		return "UNKNOWN";
	}
}