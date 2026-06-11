#include "asynclogger/log_message.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace asynclogger {
    namespace {     // 匿名命名空间
        //匿名命名空间里的函数、变量具有内部链接（internal linkage），等价于用 static 修饰的全局函数/变量。
        //它们只能在当前翻译单元（即当前 .cpp 文件）内被访问，其它 .cpp 文件即使 extern 声明也链接不到，完全不会产生符号冲突。

        //把 time_t（从 1970-01-01 开始的秒数）转换成本地时间的 std::tm 结构体（年、月、日、时、分、秒等）。
        std::tm to_local_time(std::time_t time) {
            std::tm result{};

#ifdef _WIN32
            localtime_s(&result, &time);
#else
            localtime_r(&time, &result);
#endif
//localtime_r（POSIX）和 localtime_s（Windows）是线程安全版本，调用者提供 std::tm 对象来接收结果。

            return result;
        }
    }

    std::string format_log_message(const LogMessage& message) {
        const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(message.timestamp);
        //message.timestamp 是一个 std::chrono::time_point，可能精度很高（微秒甚至纳秒）。
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(message.timestamp - seconds);

        const std::time_t raw_time = std::chrono::system_clock::to_time_t(message.timestamp);
        //system_clock::to_time_t 将 time_point 转为 C 风格的 time_t。
        const std::tm local_time = to_local_time(raw_time);

        std::ostringstream thread_id;
        thread_id << message.thread_id;

        std::ostringstream output;
        //ostringstream 是内存字符串流，用来格式化生成字符串；它本身和并发安全无关，但如果在每个线程内局部使用，能避免输出行内交错，从而辅助实现整洁的日志输出。
        output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
        << '.' <<std::setw(3)<<std::setfill('0')<<milliseconds.count()
        <<" ["<<to_string(message.level)<<"]"
        <<" [tid=" << thread_id.str()<<"] "
        <<message.text
        <<'\n';

        return output.str();
    }
}