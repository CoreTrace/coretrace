// SPDX-License-Identifier: Apache-2.0

#include "Process/Tools/RunDirectory.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    struct TestReport
    {
        int failures = 0;

        void expect(bool condition, const std::string& message)
        {
            if (condition)
            {
                std::cout << "[PASS] " << message << "\n";
                return;
            }
            ++failures;
            std::cerr << "[FAIL] " << message << "\n";
        }
    };
} // namespace

int main()
{
    TestReport report;

    // 1. The directory exists right after construction, is under temp_directory_path(),
    // and its name starts with the given prefix followed by '-'
    std::filesystem::path created_path;
    {
        ctrace::RunDirectory run_dir("testprefix");
        created_path = run_dir.path();

        report.expect(std::filesystem::exists(created_path) &&
                          std::filesystem::is_directory(created_path),
                      "RunDirectory: directory exists right after construction");

        const auto temp_root = std::filesystem::temp_directory_path();
        const bool under_temp = (created_path.string().find(temp_root.string()) == 0);
        report.expect(under_temp,
                      "RunDirectory: located under std::filesystem::temp_directory_path()");

        const std::string filename = created_path.filename().string();
        report.expect(filename.rfind("testprefix-", 0) == 0,
                      "RunDirectory: name starts with given prefix followed by '-'");
    }

    // 2. Two instances created with the same prefix get two different paths
    {
        ctrace::RunDirectory dir1("sameprefix");
        ctrace::RunDirectory dir2("sameprefix");
        report.expect(dir1.path() != dir2.path(),
                      "RunDirectory: two instances with same prefix get different paths");
    }

    // 3. The directory is removed when the object is destroyed, including when it contains files/subdirs
    {
        std::filesystem::path nested_path;
        {
            ctrace::RunDirectory run_dir("cleanup");
            nested_path = run_dir.path();

            // Create files and subdirectories inside it
            std::filesystem::create_directories(nested_path / "sub" / "dir");
            std::ofstream(nested_path / "sub" / "dir" / "file.txt") << "test data";

            report.expect(std::filesystem::exists(nested_path / "sub" / "dir" / "file.txt"),
                          "RunDirectory: files and subdirectories successfully created");
        }
        report.expect(!std::filesystem::exists(nested_path),
                      "RunDirectory: directory and its contents are removed on destruction");
    }

    if (report.failures == 0)
    {
        std::cout << "run_directory_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " run directory check(s) failed\n";
    return 1;
}
