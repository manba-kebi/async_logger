# async_logger

`async_logger` 是一个基于 C++17 的轻量异步文件日志库。

它把业务线程中的“写文件”动作拆成两步：调用线程只负责把日志消息提交到有界队列，后台 worker 线程负责格式化、写入文件、刷新和按大小滚动日志文件。这个项目适合学习和演示异步日志系统中的生产者/消费者模型、条件变量、后台线程生命周期、过载策略和文件滚动。

## 特性

- 基于 C++17 和标准库实现，无第三方运行时依赖
- 有界线程安全队列，避免日志堆积无限占用内存
- 单后台 worker 异步写文件，降低业务线程在磁盘 I/O 上的阻塞
- 支持 `INFO`、`WARN`、`ERROR` 三种日志级别
- 支持 `Block` 和 `Drop` 两种队列满时的过载策略
- 支持按文件大小滚动日志，例如 `app.log`、`app.log.1`
- 提供 `flush()` 和 `stop()`，便于明确控制日志落盘和线程退出
- 使用 CMake 构建，并提供基础测试和示例程序

## 目录结构

```text
.
├── include/asynclogger/    # 对外头文件
├── src/                    # 库实现
├── examples/               # 使用示例
├── tests/                  # CTest 测试
├── benchmarks/             # benchmark 入口，当前为占位
├── docs/                   # 设计笔记
└── CMakeLists.txt          # CMake 构建入口
```

## 快速开始

### 构建

```bash
git clone https://github.com/manba-kebi/async_logger.git
cd async_logger

cmake -S . -B build
cmake --build build --target basic_usage
```

### 运行测试

```bash
cmake --build build --target blocking_queue_test formatter_test logger_test
ctest --test-dir build --output-on-failure
```

### 运行示例

```bash
cmake --build build --target basic_usage
```

可执行文件位置取决于你的 CMake 生成器。单配置生成器通常在 `build/basic_usage`，Visual Studio 等多配置生成器通常在 `build/Debug/basic_usage.exe`。

## 使用示例

```cpp
#include "asynclogger/async_logger.h"

int main() {
    asynclogger::LoggerConfig config;
    config.file_path = "logs/basic_usage.log";
    config.max_queue_size = 1024;
    config.roll_size_bytes = 10 * 1024 * 1024;
    config.overflow_policy = asynclogger::OverflowPolicy::Block;

    asynclogger::AsyncLogger logger(config);

    logger.info("server started");
    logger.warn("queue is nearly full");
    logger.error("bind failed");

    logger.flush();
    return 0;
}
```

日志输出格式示例：

```text
2026-06-18 20:15:30.123 [INFO] [tid=12345] server started
```

## 作为子项目引入

如果你的项目也使用 CMake，可以把本项目作为子目录引入：

```cmake
add_subdirectory(async_logger)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE asynclogger)
```

然后在代码中包含头文件：

```cpp
#include "asynclogger/async_logger.h"
```

## 配置项

`AsyncLogger` 通过 `LoggerConfig` 配置：

| 配置项 | 默认值 | 说明 |
| :-: | :-: | :-: |
| `file_path` | `logs/app.log` | 基础日志文件路径。父目录不存在时会自动创建。 |
| `max_queue_size` | `1024` | 生产者线程和后台 worker 之间的有界队列容量。传入 `0` 时队列容量会被修正为 `1`。 |
| `roll_size_bytes` | `10 * 1024 * 1024` | 单个日志文件的滚动阈值，单位为字节。设置为 `0` 表示禁用滚动。 |
| `overflow_policy` | `OverflowPolicy::Block` | 队列满时的处理策略。 |
| `auto_flush` | `false` | 是否每写入一条日志后立即刷新文件流。 |

## 过载策略

当日志写入速度跟不上业务线程提交速度时，队列可能被写满。此时可以通过 `overflow_policy` 控制行为：

| 策略 | 行为 | 适用场景 |
| --- | --- | --- |
| `OverflowPolicy::Block` | 队列满时，调用线程等待直到队列有空间。 | 不能主动丢日志的场景，例如关键审计日志。 |
| `OverflowPolicy::Drop` | 队列满时，立即返回 `false` 并累计 `dropped_count()`。 | 更关注业务线程延迟，允许高峰期丢弃部分日志的场景。 |

`log()`、`info()`、`warn()`、`error()` 的返回值表示消息是否成功被接受进队列；它不等价于“已经写入磁盘”。如果需要等待已提交消息写完，可以调用 `flush()`。

## 生命周期语义

- `AsyncLogger` 构造时启动后台 worker 线程。
- `info()`、`warn()`、`error()` 会创建 `LogMessage` 并提交到队列。
- `flush()` 会等待当前已提交的日志处理完成，然后刷新文件流。
- `stop()` 会停止接收新日志，关闭队列，等待后台 worker 消费完队列中的消息并退出。
- 析构函数会调用 `stop()`，正常作用域退出时会自动收尾。

## 内部数据流

```mermaid
flowchart LR
    A["业务线程"] --> B["AsyncLogger::log()"]
    B --> C["BlockingQueue<LogMessage>"]
    C --> D["后台 worker"]
    D --> E["format_log_message()"]
    E --> F["LogFile::write()"]
    F --> G["日志文件"]
```

主要模块职责：

| 模块 | 职责 |
| :-: | :-: |
| `LogLevel` | 表达日志级别并转换为文本。 |
| `LogMessage` | 保存一条日志的结构化数据，包括级别、文本、时间戳和线程 ID。 |
| `format_log_message()` | 将结构化日志消息格式化为一行文本。 |
| `BlockingQueue<T>` | 在线程之间安全传递任务，并支持容量限制和关闭语义。 |
| `LogFile` | 管理日志文件打开、写入、刷新和按大小滚动。 |
| `AsyncLogger` | 组织生产者、队列、后台线程、文件写入和生命周期。 |

## 当前限制

- 日志格式固定，暂不支持自定义 pattern。
- 当前只有一个后台 worker。
- 文件滚动只按大小触发，不支持按日期滚动。
- 不压缩、不清理旧日志文件。
- 日志文件打开时使用覆盖写入模式，重新运行程序会覆盖同名基础日志文件。
- 文件写入错误当前没有错误回调或状态查询接口，生产使用前建议补充错误处理。
- `flush()` 更适合在没有持续并发提交的收尾阶段使用。
- `benchmark_logger` 目标当前还是占位入口，补充 `main()` 前不建议作为默认构建目标。

## 后续可改进方向

- 增加日志格式自定义能力
- 增加按日期滚动和旧日志清理策略
- 增加日志写入失败的错误回调或状态上报
- 增加更多 benchmark 数据和吞吐/延迟对比
- 增加安装规则，例如 `install()` 和导出的 CMake package
