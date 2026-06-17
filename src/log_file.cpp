#include "asynclogger/log_file.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace asynclogger {
	LogFile::LogFile(std::filesystem::path file_path, std::uintmax_t roll_size_bytes)
		:base_path_(std::move(file_path)),
		current_path_(base_path_),
		roll_size_bytes_(roll_size_bytes){
		if (base_path_.empty()) {
			throw std::invalid_argument("log file path must not be empty");
		}
	}

	LogFile::~LogFile() {
		flush();
	}

	void LogFile::write(std::string_view line) {
		open_if_needed();					// 1) 确保流已经打开
		roll_if_needed(line.size());	// 2) 如果需要，滚动到新文件

		stream_.write(line.data(), static_cast<std::streamsize>(line.size()));
		// std::ofstream::write 接受 const char* 和字符数量。
		// line.data() 返回 string_view 底层字符指针，line.size() 是字节数（注意这里假设 char 类型，适合二进制/文本日志）。
		// static_cast<std::streamsize> 是为了匹配 write 要求的类型（通常是带符号整数）。

		if (!stream_) {
			throw std::runtime_error("failed to write log file: " + current_path_.string());
		}
		
		current_size_ += static_cast<std::uintmax_t>(line.size());
		// 将写入的字节数累加到当前文件大小计数器，后续滚动判断依赖该值。
	}

	void LogFile::flush() {
		if (stream_.is_open()) {
			stream_.flush();
		}
	}

	const std::filesystem::path& LogFile::current_path() const noexcept {
		return current_path_;
	}

	std::size_t LogFile::roll_index() const noexcept {
		return roll_index_;
	}
	void LogFile::open_if_needed() {
		if (stream_.is_open()) {
			return;
		}

		if (base_path_.has_parent_path()) {
			//has_parent_path() 判断路径是否包含父级目录（例如 logs/mylog.txt 有父路径 logs）
			// 如果存在则调用 create_directories 递归创建所有必要目录，避免因目录缺失而打开失败。
			std::filesystem::create_directories(base_path_.parent_path());
		}

		current_path_ = base_path_;
		current_size_ = 0;
		stream_.open(current_path_, std::ios::out | std::ios::trunc | std::ios::binary);
		// std::ios::out：输出模式。
		// std::ios::trunc：若文件已存在，将其长度截为 0（即覆盖旧内容）。
		// std::ios::binary：禁止换行符转换（在 Windows 上 \n 不会自动转为 \r\n），保持数据原样。
		if (!stream_) {
			throw std::runtime_error("failed to open log file: " + current_path_.string());
		}
	}

	void LogFile::roll_if_needed(std::size_t next_write_size) {
		if (roll_size_bytes_ == 0) {
			//若滚动阈值设为 0，则无大小限制，永远不滚动，直接返回。
			return;
		}

		if (current_size_ == 0 || current_size_ + next_write_size <= roll_size_bytes_) {
			// 如果当前文件还是空的（刚打开），或者当前已用空间加上本条日志长度依然不超过阈值，则不需要滚动。
			// 注意：这个判断有一个隐含特性——单条日志的长度即使超过阈值，也不会触发滚动（因为 current_size_ == 0 时直接返回）。
			// 这意味着单条超长的日志行会直接写入当前文件，可能导致文件大小略微超出 roll_size_bytes_。
			return;
		}//如果当前文件非空 并且已用空间+本条日志长度超过阈值，则输出文件流对象会切换到新的文件路径
		flush();
		stream_.close();

		++roll_index_;						// 滚动计数加 1
		current_path_ = make_roll_path();	// 生成新文件路径，形如 "base_path.1"
		current_size_ = 0;					// 新文件从 0 字节开始
		stream_.open(current_path_, std::ios::out | std::ios::trunc | std::ios::binary);
		if (!stream_) {
			throw std::runtime_error("failed to open rolled log file: " + current_path_.string());
		}
	}
	
	std::filesystem::path LogFile::make_roll_path() const {
		return std::filesystem::path(base_path_.string() + "." + std::to_string(roll_index_));
		// 将基础路径转换为字符串，追加一个点号 . 和当前滚动索引（roll_index_）的字符串形式，然后构造新的 path 对象。
		// 举例：若基础路径是 logs/app.log，滚动一次后 roll_index_ 变为 1，生成 logs/app.log.1。
	}
}