#pragma once

#include <fstream>
#include <filesystem>	//文件系统库，包含 std::filesystem::path（表示文件路径）和 std::filesystem::create_directories 等函数，用于跨平台路径操作。
#include <cstddef>
#include <string_view>
#include <cstdint>		//提供固定宽度及最大宽度的整数类型，比如 std::uintmax_t（平台支持的最大无符号整数类型，通常用于表示文件大小等可能很大的尺寸）。

namespace asynclogger {
	class LogFile {
	private:
		void open_if_needed();
		//返回当前正在写入的文件的完整路径
		void roll_if_needed(std::size_t next_write_size);
		//根据即将写入的字节数 next_write_size 判断是否需要滚动：如果当前文件大小加上新数据会超过 roll_size_bytes_，则关闭当前文件，递增滚动索引并打开新文件。
		std::filesystem::path make_roll_path() const;
		//根据当前滚动索引生成新的文件路径，例如 base_path + "." + 索引。
		//std::filesystem::path 这是路径类，可以存储、拼接、分解路径。

		std::filesystem::path base_path_;		// 原始传入的基础路径
		std::filesystem::path current_path_;	// 当前实际写入文件的路径（随滚动改变）
		std::ofstream stream_;					// 输出文件流对象
		std::uintmax_t current_size_{ 0 };		// 当前文件已经写入的字节数。
		std::uintmax_t roll_size_bytes_{ 0 };	// 滚动阈值
		std::size_t roll_index_{ 0 };			// 已滚动次数，也用作文件名后缀

	public:
		LogFile(std::filesystem::path file_path, std::uintmax_t roll_size_bytes);
		//接受日志文件的基础路径 file_path，以及滚动大小阈值 roll_size_bytes
		// （超过该大小时会创建新的日志文件，0 表示永不滚动）。
		~LogFile();

		LogFile(const LogFile&) = delete;
		LogFile& operator=(const LogFile&) = delete;
		//日志文件对象通常独占底层文件句柄，拷贝没有合理语义，所以直接禁止。

		void write(std::string_view line);
		//核心写接口：接受一条日志行（以 string_view 传入，避免拷贝），负责打开文件（若未打开）、必要时执行滚动，然后将数据写入流并更新内部大小计数器。
		void flush();
		//强制将文件流缓冲区中的数据刷到磁盘。

		const std::filesystem::path& current_path() const noexcept;
		//返回当前正在写入的文件的完整路径
		std::size_t roll_index() const noexcept;
		//已经发生滚动的次数（初始为 0，每滚动一次加 1）。
	};
}