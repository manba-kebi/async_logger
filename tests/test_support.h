#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace test {
    inline void require(bool condition,std::string_view message) {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    template<typename Left,typename Right>
    void require_equal(
        const Left& actual,
        const Right& expected,
        std::string_view message
    ) {
        if (!(actual == expected)) {
            std::ostringstream output;
            output
                <<message
                <<",actual="<<actual
                <<",expected="<<expected;
            throw std::runtime_error(output.str());
        }
    }

    template<typename Function>
    int run(std::string_view name,Function function) {
        try {
            function();         //执行一段测试逻辑
            std::cout<<"[PASS] "<<name<<'\n';
            return 0;
        }catch (const std::exception& ex) {
            std::cerr<<"[FAIL] "<<name<<": "<<ex.what()<<'\n';       //what()代表异常的说明信息
            return 1;
        }catch (...) {
            std::cerr<<"[FAIL] "<<name<<": Unknown exception.\n";
            return 1;
        }
    }

    inline void reset_directory(const std::filesystem::path& path) {
        std::error_code error;      //error_code 是用来表示错误信息的一种轻量级对象，可以理解为一个“错误编号 + 错误类别”的组合
        std::filesystem::remove_all(path,error);        //remove_all 递归删除 path 及其里面所有内容。
        //如果删除失败（比如目录不存在、权限不足），错误信息会写入 error，但函数不抛异常（因为我们用了带 error_code 的版本）。
        error.clear();

        if (!std::filesystem::create_directory(path,error) && error) {
            //path：要创建的目录路径。
            //error：std::error_code& 输出参数，用来接收错误信息（如果创建失败）。
            //create_directory的返回值  返回 true：目录被成功创建了。
                                    //返回 false：目录没有被创建，原因可能是： 目录已经存在（此时 error 通常为无错误状态）。
                                                                    //  创建失败（权限不足、父目录不存在等，此时 error 会包含具体的错误码）。
            throw std::runtime_error(
                "failed to create test directory: "+path.string()
                );
        }
    }

    inline std::string read_text_file(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("failed to open test file: "+path.string());
        }

        std::ostringstream buffer;
        buffer<<input.rdbuf();          //把整个文件的内容一次性地读进 buffer（一个内存字符串），
        return buffer.str();            //最后通过 buffer.str() 得到完整的文件内容字符串。
    }

    inline std::size_t count_lines(std::string_view text) {
        std::size_t count = 0;
        for (const char character:text) {
            if (character == '\n') {
                ++count;
            }
        }
        return count;
    }
}

