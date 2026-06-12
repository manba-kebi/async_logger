#include "asynclogger/async_logger.h"

#include <chrono>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

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

    std::string parse_string_arg(int argc,char* argv[],const std::string& name,std::string default_value) {
        for (int i = 1;i+1<argc;i++) {
            if (argv[i] == name) {
                return argv[i+1];
            }
        }
        return default_value;
    }
}   //namespace

int main(int argc,char* argv[]) {
    const int thread_count = parse_int_arg(argc,argv,"--threads",4);        //线程编号
    const int messages_per_thread = parse_int_arg(argc,argv,"--messages",100000);       //每个线程要发送的日志条数
    const int queue_size = parse_int_arg(argc,argv,"--queue",8192);
    const std::string policy = parse_string_arg(argc,argv,"--policy","block");

    asynclogger::LoggerConfig config;
    config.file_path = "logs/benchmark.log";
    config.max_queue_size = static_cast<std::size_t>(queue_size);
    config.roll_size_bytes = 128*1024*1024;
    config.overflow_policy=policy == "drop" ?asynclogger::OverflowPolicy::Drop : asynclogger::OverflowPolicy::Block;

    asynclogger::AsyncLogger logger(config);

    const auto started_at = std::chrono::system_clock::now();

    std::vector<std::thread> threads;
    
    //启动一个线程，内部循环 messages_per_thread 次，用编号 i 和循环 n 拼接一条 info 日志，写入共享的 logger。
    for (int i = 0;i<thread_count;i++) {
        threads.emplace_back([i,messages_per_thread,&logger] {
            //按引用捕获。logger 是 AsyncLogger 对象，多个线程需要共享同一个日志器对象，所以用引用捕获（不会复制一份新日志器）。
            for (int n = 0;n<messages_per_thread;++n) {
                logger.info("worker=" + std::to_string(i) + " message=" + std::to_string(n));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    logger.stop();

    const auto finished_at = std::chrono::system_clock::now();
    const double seconds = std::chrono::duration<double>(finished_at-started_at).count();
    const auto total_messages = static_cast<long long>(thread_count) * messages_per_thread;

    std::cout<<"threads="<<thread_count<<'\n'
    <<"messages="<<total_messages<<'\n'
    <<"seconds="<<seconds<<'\n'
    <<"logs_per_second="<<static_cast<long long>(total_messages/seconds)<<'\n'
    <<"dropped="<<logger.dropped_count()<<'\n';

    return 0;
}