cmake_minimum_required(VERSION 3.16)

project(async_logger VERSION 0.1.0 LANGUAGES CXX)
# 定义项目版本，后续可以用于生成版本头文件、打包、find_package 时要求版本范围等

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 自动识别是否为“顶层项目”
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	set(ASYNCLOGGER_IS_TOP_LEVEL ON)
else()
	set(ASYNCLOGGER_IS_TOP_LEVEL OFF)
endif()

# 当作为顶层项目时默认开启，作为子项目时默认关闭。
option(
		ASYNCLOGGER_BUILD_EXAMPLES
		"Build async_logger examples"
		${ASYNCLOGGER_IS_TOP_LEVEL}
)

option(
		ASYNCLOGGER_BUILD_TESTS
		"Build async_logger tests"
		${ASYNCLOGGER_IS_TOP_LEVEL}
)

find_package(Threads REQUIRED)

add_library(asynclogger
	src/log_level.cpp
	src/log_message.cpp
	src/log_file.cpp
	src/async_logger.cpp
)

# 为目标 asynclogger 创建了一个带命名空间的别名 asynclogger::asynclogger
add_library(asynclogger::asynclogger ALIAS asynclogger)

# 用生成器表达式 $<BUILD_INTERFACE:...> 和 $<INSTALL_INTERFACE:...> 区分构建时和安装后的头文件搜索路径。
target_include_directories(asynclogger 
	PUBLIC 
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(asynclogger
		PUBLIC
			Threads::Threads
)

if(MSVC)
	target_compile_options(asynclogger PRIVATE /W4 /permissive- /utf-8)
else()
	target_compile_options(asynclogger PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ASYNCLOGGER_BUILD_EXAMPLES)
	add_executable(async_logger_basic_usage examples/basic_usage.cpp
			tests/test_support.h)
	target_link_libraries(
			async_logger_basic_usage
			PRIVATE
				asynclogger::asynclogger
	)
endif ()

if(ASYNCLOGGER_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
