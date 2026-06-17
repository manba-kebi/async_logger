#include "asynclogger/log_file.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "test_support.h"

namespace {

    const std::filesystem::path kRoot = std::filesystem::path("test_output")/"log_file";

    //测试空路径输入是否正常报错
    void test_empty_path_is_rejected() {
        bool threw = false;

        try {
            asynclogger::LogFile file("",1024);
        }catch (const std::invalid_argument&) {
            threw = true;
        }

        test::require(threw,"empty log path must throw invalid_argument");
    }

    //测试父文件夹未创建情况下，向子文件写入内容，是否正常创建父文件夹（具体实现在open_if_needed函数中）
    //并且测试写入的内容和读取的内容是否一致
    void test_parent_directoies_are_created() {
        test::reset_directory(kRoot);

        const auto path = kRoot / "nested" /"app.log";

        {
            asynclogger::LogFile file(path,1024);
            file.write("hello\n");
            file.flush();
        }

        test::require(std::filesystem::exists(path),"LogFile must create missing parent directories");
        test::require_equal(test::read_text_file(path),std::string("hello\n"),"written file content is incorrect");
    }

    //测试滚动大小为 0 时，写入多个字节的内容，滚动次数是否为0
    void test_zero_roll_size_disables_rotation() {
        test::reset_directory(kRoot);

        const auto path  = kRoot / "no_roll.log";

        {
            asynclogger::LogFile file(path,0);
            file.write("first\n");
            file.write("second\n");
            file.flush();

            test::require_equal(file.roll_index(),std::size_t{0},"roll index must remain zero when rotation is disabled");

            test::require(!std::filesystem::exists(path.string()+".1"),"rotation file must not exist when threshold is zero");
        }
    }

    //测试分别写入的内容的大小 在刚好达到滚动阈值的情况下：是否不会滚动到下一个文件，是否内容全正常写入到同一个文件中了
    void test_exact_boundary_does_not_rotate() {
        test::reset_directory(kRoot);

        const auto path = kRoot / "boundary.log";

        asynclogger::LogFile file(path,6);
        file.write("abc");
        file.write("def");
        file.flush();

        test::require_equal(file.roll_index(),std::size_t{0},"exact threshold must stay in the current file");
        test::require(!std::filesystem::exists(path.string() + ".1"),"exact threshold must not create a rolled file");
        test::require_equal(test::read_text_file(path),std::string("abcdef"),"base file content is incorrect at exact boundary");
    }

    //测试分别写入的内容 的大小超过滚动阈值后能否 正常 滚动到下一个文件，包括文件的内容与命名是否正确
    void test_next_write_rotates_before_writing() {
        test::reset_directory(kRoot);

        const auto path = kRoot / "rotate.log";

        asynclogger::LogFile file(path,6);
        file.write("abc");
        file.write("def");
        file.write("g");
        file.flush();

        const auto rolled_path = std::filesystem::path(path.string()+ ".1");

        test::require_equal(file.roll_index(),size_t{1},"roll index must increment after rotation");
        test::require_equal(file.current_path(),rolled_path,"current path must point to the rolled file");
        test::require_equal(test::read_text_file(path),std::string("abcdef"),"base file content must remain before rotation");
        test::require_equal(test::read_text_file(rolled_path),std::string("g"),"new data must be written to the rolled file");
    }

    //测试 输入多个字节（大小超过两倍的滚动阈值） 能否正常按顺序滚动到下一个文件，在滚动到下一个文件
    void test_multiple_rotations_increment_suffix() {
        test::reset_directory(kRoot);

        const auto path = kRoot / "multiple.log";

        asynclogger::LogFile file(path,3);
        file.write("aaa");
        file.write("bbb");
        file.write("ccc");
        file.flush();

        test::require_equal(file.roll_index(),std::size_t{2},"three full chunks must occupy base, .1 and .2");
        test::require(std::filesystem::exists(path.string()+".1"),"first rolled file must exist");
        test::require(std::filesystem::exists(path.string()+".2"),"second rolled file must exist");
    }
}

int main() {
    int failures = 0;

    failures += test::run("empty path is rejected",test_empty_path_is_rejected);
    failures += test::run("parent directories are created",test_parent_directoies_are_created);
    failures += test::run("zero roll size disables rotation",test_zero_roll_size_disables_rotation);
    failures += test::run("exact boundary does not rotate",test_exact_boundary_does_not_rotate);
    failures += test::run("next write rotates before writing",test_next_write_rotates_before_writing);
    failures += test::run("multiple rotations increment suffix",test_multiple_rotations_increment_suffix);

    return failures == 0 ? 0:1;
}
