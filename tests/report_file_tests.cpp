// SPDX-License-Identifier: Apache-2.0
//
// Writing a report file: the directories are created, the previous content is replaced, and
// every failure is returned with its reason instead of thrown.
#include "Process/Tools/ReportFile.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <unistd.h>

namespace
{
    namespace fs = std::filesystem;

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

    std::string contentOf(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream content;
        content << in.rdbuf();
        return content.str();
    }

    void testWritesThroughMissingDirectories(TestReport& report, const fs::path& base)
    {
        const fs::path target = base / "nested" / "deeper" / "report.json";
        std::string error;
        report.expect(ctrace::writeReportToFile(target.string(), "{\"ok\": true}", error) &&
                          error.empty(),
                      "missing parent directories are created");
        report.expect(contentOf(target) == "{\"ok\": true}", "the content is written as given");
    }

    void testReplacesALongerReport(TestReport& report, const fs::path& base)
    {
        const fs::path target = base / "replaced.json";
        std::string error;
        (void)ctrace::writeReportToFile(target.string(), "a much longer previous report", error);
        report.expect(ctrace::writeReportToFile(target.string(), "short", error) &&
                          contentOf(target) == "short",
                      "a new report replaces the previous one entirely");
    }

    void testEmptyPathIsAnError(TestReport& report)
    {
        std::string error;
        report.expect(!ctrace::writeReportToFile("", "content", error) &&
                          error == "report path is empty",
                      "an empty path is refused with a reason");
    }

    void testFileInPlaceOfADirectory(TestReport& report, const fs::path& base)
    {
        const fs::path blocker = base / "blocker";
        std::ofstream(blocker) << "a file, not a directory";
        std::string error;
        report.expect(!ctrace::writeReportToFile((blocker / "report.json").string(), "x", error) &&
                          error.find("failed to create report directory") != std::string::npos,
                      "a parent that is a file is reported (" + error + ")");
    }

    void testDirectoryInPlaceOfTheFile(TestReport& report, const fs::path& base)
    {
        const fs::path directory = base / "a-directory";
        fs::create_directories(directory);
        std::string error;
        report.expect(!ctrace::writeReportToFile(directory.string(), "x", error) &&
                          error.find("failed to open report file") != std::string::npos,
                      "a path that is a directory is reported (" + error + ")");
    }
} // namespace

int main()
{
    TestReport report;
    const fs::path base =
        fs::temp_directory_path() / ("ctrace-report-file-" + std::to_string(::getpid()));
    fs::remove_all(base);
    fs::create_directories(base);

    testWritesThroughMissingDirectories(report, base);
    testReplacesALongerReport(report, base);
    testEmptyPathIsAnError(report);
    testFileInPlaceOfADirectory(report, base);
    testDirectoryInPlaceOfTheFile(report, base);

    fs::remove_all(base);
    if (report.failures == 0)
    {
        std::cout << "report_file_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " report file check(s) failed\n";
    return 1;
}
