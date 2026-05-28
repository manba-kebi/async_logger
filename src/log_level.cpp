#include "asynclogger/log_level.h"

namespace asynclogger {
	////°Ñ LogLevel ×ª³É×Ö·û´®
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