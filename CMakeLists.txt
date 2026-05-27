cmake_minimum_required(VERSION 3.16)

project(async_logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(asynclogger
	src/log_level.cpp
	src/log_message.cpp
	src/log_file.cpp
	src/async_logger.cpp
)

target_include_directories(asynclogger 
	PUBLIC 
		${PROJECT_SOURCE_DIR}/include
)

if(MSVC)
	target_compile_options(asynclogger PRIVATE /W4 /permissive)
else()
	target_compile_options(asynclogger PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(basic_usage examples/basic_usage.cpp)
target_link_libraries(basic_usage PRIVATE asynclogger)

enable_testing()

function(add_async_logger_test target_name source_file)
	add_executable(${target_name} ${source_file})
	target_link_libraries(${target_name} PRIVATE asynclogger)
	add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_async_logger_test(blocking_queue_test tests/blocking_queue_test.cpp)
add_async_logger_test(formatter_test tests/formatter_test.cpp)
add_async_logger_test(logger_test tests/logger_test.cpp)

add_executable(benchmark_logger benchmarks/benchmark_logger.cpp)
target_link_libraries(benchmark_logger PRIVATE asynclogger)