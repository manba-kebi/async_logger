# Benchmark 说明

这份文档说明 `benchmarks/benchmark_logger.cpp` 的构建、运行、输出字段和结果解释方式。

当前 benchmark 是一个轻量压测入口，主要用于比较不同线程数、队列大小和过载策略下的日志提交表现。它不是严格的微基准框架，也不提供延迟分位数统计。

## 构建

benchmark 默认不构建，需要显式打开 `ASYNCLOGGER_BUILD_BENCHMARKS`。

推荐使用单独构建目录。

Ninja/Makefile 等单配置生成器：

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DASYNCLOGGER_BUILD_TESTS=OFF -DASYNCLOGGER_BUILD_EXAMPLES=OFF -DASYNCLOGGER_BUILD_BENCHMARKS=ON
cmake --build build-bench --target benchmark_logger
```

Visual Studio 生成器是多配置生成器，`CMAKE_BUILD_TYPE=Release` 不控制最终配置，需要在构建时显式指定 `--config Release`：

```powershell
cmake -S . -B build-bench -DASYNCLOGGER_BUILD_TESTS=OFF -DASYNCLOGGER_BUILD_EXAMPLES=OFF -DASYNCLOGGER_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --target benchmark_logger
```

如果省略 `--config Release`，Visual Studio 通常会构建 Debug，可执行文件会在 `build-bench\Debug\benchmark_logger.exe`。

## 运行

Ninja/Makefile 或 Git Bash 下：

Block 策略示例：

```bash
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 8192 --policy block --log logs/benchmark-block.log
```

Drop 策略示例：

```bash
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 1024 --policy drop --log logs/benchmark-drop.log
```

Windows `cmd.exe` 或 PowerShell 下不要使用 `./build-bench/...`，要使用反斜杠路径。

如果前面构建的是 Release：

```powershell
.\build-bench\Release\benchmark_logger.exe --threads 4 --messages 100000 --queue 8192 --policy block --log logs\benchmark-block.log
```

如果前面没有加 `--config Release`，实际生成的是 Debug：

```powershell
.\build-bench\Debug\benchmark_logger.exe --threads 4 --messages 100000 --queue 8192 --policy block --log logs\benchmark-block.log
```

Debug 版本只适合确认程序能启动，性能数字没有参考价值。正式压测请先构建 Release：

```powershell
cmake --build build-bench --config Release --target benchmark_logger
```

## 常见运行问题

如果在 Windows `cmd.exe` 中看到：

```text
'.' 不是内部或外部命令，也不是可运行的程序或批处理文件。
```

说明执行了 `./build-bench/...` 这种 bash 路径写法。`cmd.exe` 下改用：

```cmd
.\build-bench\Release\benchmark_logger.exe --threads 4 --messages 100000 --queue 8192 --policy block --log logs\benchmark-block.log
```

如果看到：

```text
系统找不到指定的路径。
```

通常是可执行文件不在你写的配置目录里。检查实际生成位置：

```cmd
dir build-bench\Debug\benchmark_logger.exe
dir build-bench\Release\benchmark_logger.exe
```

如果只有 `build-bench\Debug\benchmark_logger.exe`，说明前面构建的是 Debug。要么临时运行 Debug 路径，要么重新构建 Release。

如果 CMake 报 `CMakeCache.txt directory ... is different than ...`，说明 `build-bench` 是在另一个源码目录下生成的。换目录或复制项目后，不要复用旧构建目录，建议删除 `build-bench` 后重新配置，或直接换一个新目录，例如：

```powershell
cmake -S . -B build-bench-release -DASYNCLOGGER_BUILD_TESTS=OFF -DASYNCLOGGER_BUILD_EXAMPLES=OFF -DASYNCLOGGER_BUILD_BENCHMARKS=ON
cmake --build build-bench-release --config Release --target benchmark_logger
.\build-bench-release\Release\benchmark_logger.exe --threads 4 --messages 100000 --queue 8192 --policy block --log logs\benchmark-block.log
```

## 参数

| 参数 | 默认值 | 说明 |
| :-: | :-: | :-: |
| `--threads` | `4` | 生产者线程数量。 |
| `--messages` | `100000` | 每个生产者线程提交的日志条数。 |
| `--queue` | `8192` | logger 内部队列容量。 |
| `--policy` | `block` | 队列满时策略。可选 `block` 或 `drop`，其他值会报错并退出。 |
| `--log` | `logs/benchmark.log` | benchmark 输出日志文件路径。 |

总尝试提交次数为：

```text
attempted = threads * messages
```

## 输出字段

benchmark 会输出类似下面的字段：

```text
threads=4
messages_per_thread=100000
attempted=400000
accepted=400000
rejected_by_return_value=0
dropped_by_logger=0
policy=block
queue_size=8192
submit_seconds=2.3897
total_seconds_including_drain=2.3897
attempted_per_second=167384
accepted_per_second=167384
```

字段含义：

| 字段 | 含义 |
| :-: | :-: |
| `threads` | 生产者线程数量。 |
| `messages_per_thread` | 每个生产者线程尝试提交的日志条数。 |
| `attempted` | 所有线程调用 `logger.info()` 的总次数。 |
| `accepted` | `logger.info()` 返回 `true` 的次数。 |
| `rejected_by_return_value` | `logger.info()` 返回 `false` 的次数。 |
| `dropped_by_logger` | logger 内部累计 dropped 数。它包含队列满导致的拒绝，也包含后台写文件失败。 |
| `submit_seconds` | 从启动生产者线程到所有生产者线程提交结束的时间。 |
| `total_seconds_including_drain` | 从开始提交到 `logger.stop()` 完成的时间，包含后台 worker 消费剩余队列的时间。 |
| `attempted_per_second` | 按 `attempted / submit_seconds` 计算的提交尝试吞吐。 |
| `accepted_per_second` | 按 `accepted / submit_seconds` 计算的成功入队吞吐。 |

## 为什么要统计返回值

`AsyncLogger::info()` 的返回值表示日志是否成功被接受进入队列。

Drop 策略下，如果队列已满，`info()` 会立刻返回 `false`。这类日志没有进入队列，也不会被后台线程写入文件。如果 benchmark 只统计调用次数，就会把这些被拒绝的日志也算进吞吐，结果会偏高。

因此 benchmark 同时统计：

```text
attempted = accepted + rejected_by_return_value
```

在正常文件可写的情况下，Drop 策略下通常还应该满足：

```text
dropped_by_logger == rejected_by_return_value
```

如果 `dropped_by_logger` 大于 `rejected_by_return_value`，说明除了队列满导致的拒绝之外，还发生了后台写文件失败等问题。

## 如何解释 Block 和 Drop

Block 策略下：

- 队列满时业务线程会等待。
- `accepted` 理论上应该等于 `attempted`，除非 logger 已停止或发生异常边界情况。
- `submit_seconds` 会体现背压成本，因为生产者线程被迫等待后台 worker 消费队列。
- 适合验证“不主动丢日志”的吞吐上限。

Drop 策略下：

- 队列满时业务线程不等待。
- `attempted_per_second` 可能很高，但这不代表有效写入吞吐高。
- 必须看 `accepted`、`rejected_by_return_value` 和 `accepted_per_second`。
- 适合验证“保护业务线程延迟时，会牺牲多少日志完整性”。

## 建议的实验组合

可以按下面顺序跑，逐步观察变化。

下面命令使用 Git Bash/Ninja 路径写法。Windows `cmd.exe` 或 PowerShell 下，把开头的 `./build-bench/benchmark_logger` 替换成实际生成的可执行文件路径，例如 `.\build-bench\Release\benchmark_logger.exe`。

基础 Block：

```bash
./build-bench/benchmark_logger --threads 1 --messages 100000 --queue 8192 --policy block --log logs/bench-t1-block.log
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 8192 --policy block --log logs/bench-t4-block.log
./build-bench/benchmark_logger --threads 8 --messages 100000 --queue 8192 --policy block --log logs/bench-t8-block.log
```

不同队列大小：

```bash
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 64 --policy block --log logs/bench-q64-block.log
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 1024 --policy block --log logs/bench-q1024-block.log
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 8192 --policy block --log logs/bench-q8192-block.log
```

Drop 策略：

```bash
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 64 --policy drop --log logs/bench-q64-drop.log
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 1024 --policy drop --log logs/bench-q1024-drop.log
./build-bench/benchmark_logger --threads 4 --messages 100000 --queue 8192 --policy drop --log logs/bench-q8192-drop.log
```

对比时不要只看一个字段。建议记录：

- `attempted_per_second`
- `accepted_per_second`
- `accepted`
- `rejected_by_return_value`
- `dropped_by_logger`
- `total_seconds_including_drain`

## 压测注意事项

- 使用 Release 构建，Debug 构建的数字没有参考价值。
- 每组至少跑多次，观察波动，不要只取单次结果。
- 尽量关闭其他重磁盘 I/O 程序。
- 不同机器、不同磁盘、不同文件系统之间不要直接比较绝对数字。
- benchmark 中包含字符串拼接成本，不是纯日志库内部成本。
- 文件系统缓存会影响结果，特别是重复运行同一路径时。
- 当前 benchmark 不统计单条日志延迟、P50、P95、P99。

## 可以继续改进的方向

- 增加 `--rounds`，自动多轮运行并输出平均值。
- 增加 CSV 输出，方便画图。
- 增加延迟采样，统计 P50、P95、P99。
- 增加 payload 大小参数，例如 `--payload-bytes`。
- 增加 `--auto-flush` 参数，对比每条日志立即 flush 的成本。
- 增加 append/truncate 配置，避免重复运行覆盖基础日志文件。
