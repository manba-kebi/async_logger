# async_logger

`async_logger` 是一个基于 C++17 标准库实现的轻量级异步文件日志库。

它把业务线程里的日志提交和磁盘写入拆开：业务线程只负责把 `LogMessage` 放入有界队列，后台 worker 线程负责格式化、写文件、刷新和按大小滚动日志文件。这个项目适合学习异步日志系统里的生产者消费者模型、条件变量、线程生命周期、有界队列背压、Drop 策略和文件滚动。

## 特性

- C++17 实现，无第三方运行时依赖。
- 有界线程安全队列，避免日志堆积无限占用内存。
- 单后台 worker 异步写文件，降低业务线程直接等待磁盘 I/O 的概率。
- 支持 `INFO`、`WARN`、`ERROR` 三种日志级别。
- 支持 `Block` 和 `Drop` 两种队列满载策略。
- 支持按文件大小滚动日志，例如 `app.log`、`app.log.1`。
- 提供 `flush()` 和 `stop()`，便于显式控制日志落盘和后台线程退出。
- 提供基础单元测试、示例程序和可选 benchmark 程序。

## 目录结构

```text
.
|-- include/asynclogger/    # 对外头文件
|-- src/                    # 库实现
|-- examples/               # 使用示例
|-- tests/                  # CTest 测试
|-- benchmarks/             # benchmark 程序
|-- docs/                   # 设计、压测、面试说明
`-- CMakeLists.txt          # CMake 构建入口
```

## 快速开始

构建前需要安装 CMake 3.16 或更高版本，以及支持 C++17 的编译器。Windows 命令行如果默认生成器选择了 NMake，需要在 Visual Studio Developer PowerShell 中运行，或显式指定你自己的生成器和编译器；使用 CLion 时也可以直接用 CLion 配好的 CMake toolchain 构建。

### 构建库、示例和测试

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

如果使用 Ninja、Makefiles 这类单配置生成器，可以省略 `--config Release`，并在配置阶段指定构建类型：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### 只构建并运行示例

示例目标名是 `async_logger_basic_usage`：

```bash
cmake --build build --config Release --target async_logger_basic_usage
```

Windows Visual Studio 生成器下通常这样运行：

```powershell
.\build\Release\async_logger_basic_usage.exe
```

Ninja、Makefiles 或 Linux/macOS 下通常这样运行：

```bash
./build/async_logger_basic_usage
```

运行后会生成 `logs/basic_usage.log`。

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

日志格式示例：

```text
2026-06-18 20:15:30.123 [INFO] [tid=12345] server started
```

## 作为子项目引入

如果你的项目也使用 CMake，可以把本项目作为子目录引入：

```cmake
add_subdirectory(async_logger)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE asynclogger::asynclogger)
```

然后在代码中包含头文件：

```cpp
#include "asynclogger/async_logger.h"
```

当 `async_logger` 作为子项目引入时，示例和测试默认不会构建；顶层项目可以通过 `ASYNCLOGGER_BUILD_EXAMPLES`、`ASYNCLOGGER_BUILD_TESTS`、`ASYNCLOGGER_BUILD_BENCHMARKS` 显式控制。

## 配置项

`AsyncLogger` 通过 `LoggerConfig` 配置：

| 配置项 | 默认值 | 说明 |
| :-: | :-: | :-: |
| `file_path` | `logs/app.log` | 基础日志文件路径。父目录不存在时会自动创建。 |
| `max_queue_size` | `1024` | 生产者线程和后台 worker 之间的有界队列容量。传入 `0` 时会修正为 `1`。 |
| `roll_size_bytes` | `10 * 1024 * 1024` | 单个日志文件的滚动阈值，单位是字节。设置为 `0` 表示禁用滚动。 |
| `overflow_policy` | `OverflowPolicy::Block` | 队列满时的处理策略。 |
| `auto_flush` | `false` | 是否每写入一条日志后立即刷新文件流。 |

## 过载策略

当日志提交速度超过后台线程写入速度时，队列可能被写满。此时由 `overflow_policy` 决定行为：

| 策略 | 行为 | 适用场景 |
| :-: | :-: | :-: |
| `OverflowPolicy::Block` | 队列满时，调用线程等待直到队列有空间。 | 不能主动丢日志的场景，例如关键审计日志。 |
| `OverflowPolicy::Drop` | 队列满时，调用立刻返回 `false`，并累计 `dropped_count()`。 | 更关注业务线程延迟，允许高峰期丢弃部分日志的场景。 |

`log()`、`info()`、`warn()`、`error()` 的返回值表示消息是否成功被接受进入队列；它不等价于“已经写入磁盘”。如果需要等待已提交消息写完，可以调用 `flush()`。对象退出或显式调用 `stop()` 时，会关闭队列并等待后台 worker 消费完已经入队的消息。

## Benchmark

benchmark 是可选目标，默认不构建。建议使用 Release 构建，并单独开一个构建目录：

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DASYNCLOGGER_BUILD_TESTS=OFF -DASYNCLOGGER_BUILD_EXAMPLES=OFF -DASYNCLOGGER_BUILD_BENCHMARKS=ON
cmake --build build-bench --target benchmark_logger
```

Visual Studio 生成器可以这样构建：

```powershell
cmake -S . -B build-bench -DASYNCLOGGER_BUILD_TESTS=OFF -DASYNCLOGGER_BUILD_EXAMPLES=OFF -DASYNCLOGGER_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --target benchmark_logger
```

运行 Block 策略：

```bash
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 8192 --policy block --log logs/benchmark-block.log
```

Windows Visual Studio 生成器下：

```powershell
.\build-bench\Release\benchmark_logger.exe --threads 4 --messages 100000 --queue 8192 --policy block --log logs\benchmark-block.log
```

运行 Drop 策略：

```bash
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 1024 --policy drop --log logs/benchmark-drop.log
```

benchmark 输出中的关键字段：

| 字段 | 含义 |
| :-: | :-: |
| `attempted` | 业务线程尝试调用 `logger.info()` 的总次数。 |
| `accepted` | `logger.info()` 返回 `true` 的次数，也就是成功入队的日志数。 |
| `rejected_by_return_value` | `logger.info()` 返回 `false` 的次数。Drop 策略下必须统计这个值。 |
| `dropped_by_logger` | logger 内部累计的 dropped 计数，包含队列满导致的拒绝，也包含后台写文件失败时的计数。 |
| `submit_seconds` | 所有生产者线程完成日志提交所花的时间。 |
| `total_seconds_including_drain` | 从开始提交到 `logger.stop()` 完成的总时间，包含后台线程消费队列的收尾时间。 |
| `attempted_per_second` | 按尝试提交数计算的提交吞吐。 |
| `accepted_per_second` | 按成功入队数计算的有效吞吐。 |

更多压测方法和结果解释见 [docs/benchmark_notes.md](docs/benchmark_notes.md)。

## 文档

- [docs/design_notes.md](docs/design_notes.md)：内部设计、线程模型、生命周期和关键不变量。
- [docs/benchmark_notes.md](docs/benchmark_notes.md)：benchmark 构建命令、参数说明、字段解释和实验建议。
- [docs/interview_notes.md](docs/interview_notes.md)：面试讲解提纲、常见追问和项目改进方向。

## 当前限制

- 日志格式固定，暂不支持自定义 pattern。
- 当前只有一个后台 worker。
- 文件滚动只按大小触发，不支持按日期滚动。
- 不压缩、不清理旧日志文件。
- 基础日志文件使用覆盖写入模式，重新运行程序会覆盖同名基础日志文件。
- 文件写入错误目前只累计到 `dropped_count()`，没有错误回调或状态查询接口。
- `flush()` 更适合在没有持续并发提交的收尾阶段使用。

## 后续可改进方向

- 支持自定义日志格式。
- 支持按日期滚动和旧日志清理策略。
- 增加错误回调或状态上报接口。
- 增加延迟分位数、CSV 输出和多轮运行统计的 benchmark。
- 增加 `install()` 规则和导出的 CMake package。
