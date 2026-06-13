function(add_asynclogger_test target_name)
    add_executable(${target_name} ${target_name}.cpp)

    target_link_libraries(
            ${target_name}
            PRIVATE
                asynclogger::asynclogger
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${target_name} COMMAND ${target_name})

    set_tests_properties(
            ${target_name}
            PROPERTIES
            TIMEOUT 15
    )
endfunction()

add_asynclogger_test(blocking_queue_test)
add_asynclogger_test(formatter_test)
add_asynclogger_test(log_file_test)
add_asynclogger_test(logger_test)