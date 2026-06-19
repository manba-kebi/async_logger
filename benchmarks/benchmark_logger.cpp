#include "asynclogger/async_logger.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

    //找到命令行中 指定命令 后 紧跟着的int型参数
    int parse_int_arg(int argc,char* argv[],const std::string& name,int default_value) {
        for (int i = 1;i+1<argc;i++) {
            if (argv[i] == name) {
                return std::atoi(argv[i+1]);
                //ASCII to integer
                //会忽略前导空白，转换连续数字直到遇到非数字字符
            }
        }
        return default_value;
    }

    //找到命令行中 指定命令 后 紧跟着的string型参数
    std::string parse_string_arg(int argc,char* argv[],const std::string& name,std::string default_value) {
        for (int i = 1;i+1<argc;i++) {
            if (argv[i] == name) {
                return argv[i+1];
            }
        }
        return default_value;
    }

    //判断输入命令中 是否有 指定的参数名
    bool has_flag(int argc,char* argv[],const std::string& name) {
        for (int i = 1;i<argc ;i++) {
            if (argv[i] == name) {
                return true;
            }
        }
        return false;
    }

    //计算平均每秒的数量，就是count的平均值
    double per_second(std::uint64_t count,double seconds) {
        if (seconds <= 0.0) {
            return 0.0;
        }
        return static_cast<double>(count)/seconds;
    }

    //让用户明白这个命令后面要接的参数以及类型
    void print_usage(const char* program_name) {
        std::cout<<"Usage: "<<program_name<<" [--threads N] [--messages N] [--queue N] [--policy block|drop] [--log PATH]\n";
    }
}   //namespace

int main(int argc,char* argv[]) {
    if (has_flag(argc,argv,"--help")) {
        print_usage(argv[0]);
        return 0;
    }

    const int thread_count = parse_int_arg(argc,argv,"--threads",4);        //线程编号
    const int messages_per_thread = parse_int_arg(argc,argv,"--messages",100000);       //每个线程要发送的日志条数
    const int queue_size = parse_int_arg(argc,argv,"--queue",8192);
    const std::string policy = parse_string_arg(argc,argv,"--policy","block");
    const std::string log_path = parse_string_arg(argc,argv,"--log","logs/benchmark.log");

    //提示线程数、每个线程要发的消息数、队列大小都必须是正数
    if (thread_count <= 0 || messages_per_thread <= 0 || queue_size <= 0) {
        std::cerr<<"--threads, --messages, and --queue must all be positive integers\n";
        print_usage(argv[0]);
        return 1;
    }

    //处理策略必须是 block 或 drop 其中的一个
    if (policy != "block" && policy != "drop") {
        std::cerr << "--policy must be either 'block' or 'drop'\n";
        print_usage(argv[0]);
        return 1;
    }

    asynclogger::LoggerConfig config;
    config.file_path = log_path;
    config.max_queue_size = static_cast<std::size_t>(queue_size);
    config.roll_size_bytes = 128*1024*1024;
    config.overflow_policy=policy == "drop" ?asynclogger::OverflowPolicy::Drop : asynclogger::OverflowPolicy::Block;

    asynclogger::AsyncLogger logger(config);

    std::atomic<std::uint64_t> accepted_count{0};
    std::atomic<std::uint64_t> rejected_count{0};

    const auto submit_started_at = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(thread_count));
    
    //启动一个线程，内部循环 messages_per_thread 次，用编号 i 和循环 n 拼接一条 info 日志，写入共享的 logger。
    for (int thread_index = 0;thread_index<thread_count;thread_index++) {
        threads.emplace_back([thread_index,messages_per_thread,&logger,&accepted_count,&rejected_count] {
            //按引用捕获。logger 是 AsyncLogger 对象，多个线程需要共享同一个日志器对象，所以用引用捕获（不会复制一份新日志器）。
            for (int message_index = 0;message_index<messages_per_thread;++message_index) {
                const bool accepted = logger.info("worker=" + std::to_string(thread_index) + " message=" + std::to_string(message_index));
                if (accepted) {
                    accepted_count.fetch_add(1,std::memory_order_relaxed);
                    //std::memory_order_relaxed 是在告诉编译器：这个原子操作只保证计数本身是原子的，不需要和任何其他内存操作有先后顺序保证。
                }else {
                    rejected_count.fetch_add(1,std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.stop();

    const auto submit_finished_at = std::chrono::steady_clock::now();
    logger.stop();
    const auto drain_finished_at = std::chrono::steady_clock::now();

    const auto attempted_count = static_cast<std::uint64_t>(thread_count) * static_cast<uint64_t>(messages_per_thread);
    const std::uint64_t accepted = accepted_count.load(std::memory_order_relaxed);
    const std::uint64_t rejected = rejected_count.load(std::memory_order_relaxed);
    const std::uint64_t dropped_by_logger = logger.dropped_count();

    const double submit_seconds = std::chrono::duration<double>(
    submit_finished_at - submit_started_at
    ).count();
    const double total_seconds = std::chrono::duration<double>(
        drain_finished_at - submit_started_at
    ).count();

    std::cout
        << "threads=" << thread_count << '\n'
        << "messages_per_thread=" << messages_per_thread << '\n'
        << "attempted=" << attempted_count << '\n'
        << "accepted=" << accepted << '\n'
        << "rejected_by_return_value=" << rejected << '\n'
        << "dropped_by_logger=" << dropped_by_logger << '\n'
        << "policy=" << policy << '\n'
        << "queue_size=" << queue_size << '\n'
        << "submit_seconds=" << submit_seconds << '\n'
        << "total_seconds_including_drain=" << total_seconds << '\n'
        << "attempted_per_second=" << static_cast<std::uint64_t>(per_second(attempted_count, submit_seconds)) << '\n'
        << "accepted_per_second=" << static_cast<std::uint64_t>(per_second(accepted, submit_seconds)) << '\n';

    return 0;
}