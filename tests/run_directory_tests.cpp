// SPDX-License-Identifier: Apache-2.0
//
// RunDirectory gives each tool run a directory of its own in the temporary directory: it exists
// once constructed, two runs never share one, and it is removed with its content on destruction.
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

    // 1. The directory exists right after construction, directly in temp_directory_path(),
    // and its name starts with the given prefix followed by '-'; empty, it is removed with
    // the object.
    std::filesystem::path createdPath;
    {
        ctrace::RunDirectory runDirectory("testprefix");
        createdPath = runDirectory.path();

        report.expect(std::filesystem::is_directory(createdPath),
                      "RunDirectory: directory exists right after construction");

        // temp_directory_path() may end with a separator (macOS): compare the directories,
        // not their spellings.
        report.expect(std::filesystem::equivalent(createdPath.parent_path(),
                                                  std::filesystem::temp_directory_path()),
                      "RunDirectory: located in std::filesystem::temp_directory_path()");

        report.expect(createdPath.filename().string().starts_with("testprefix-"),
                      "RunDirectory: name starts with given prefix followed by '-'");
    }
    report.expect(!std::filesystem::exists(createdPath),
                  "RunDirectory: an empty directory is removed on destruction");

    // 2. Two instances created with the same prefix get two different directories, which
    // exist at the same time
    {
        ctrace::RunDirectory first("sameprefix");
        ctrace::RunDirectory second("sameprefix");
        report.expect(first.path() != second.path(),
                      "RunDirectory: two instances with same prefix get different paths");
        report.expect(std::filesystem::is_directory(first.path()) &&
                          std::filesystem::is_directory(second.path()),
                      "RunDirectory: both directories exist at the same time");
    }

    // 3. The directory is removed when the object is destroyed, including when it contains files/subdirs
    {
        std::filesystem::path nestedPath;
        {
            ctrace::RunDirectory runDirectory("cleanup");
            nestedPath = runDirectory.path();

            std::filesystem::create_directories(nestedPath / "sub" / "dir");
            std::ofstream(nestedPath / "sub" / "dir" / "file.txt") << "test data";

            report.expect(std::filesystem::exists(nestedPath / "sub" / "dir" / "file.txt"),
                          "RunDirectory: files and subdirectories successfully created");
        }
        report.expect(!std::filesystem::exists(nestedPath),
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
