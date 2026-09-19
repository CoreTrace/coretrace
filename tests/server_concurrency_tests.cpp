// SPDX-License-Identifier: Apache-2.0
//
// Server mode handles requests on a thread pool. Concurrent run_analysis requests must each
// come back complete and correct, with the logging configured the way run_server does it.
#include "App/Runner.hpp"
#include "Process/Ipc/HttpServer.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using json = nlohmann::json;

    struct TestReport
    {
        std::atomic<int> failures{0};

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

    json makeRequest(const std::filesystem::path& repoRoot, const std::filesystem::path& reportFile,
                     int id)
    {
        json request;
        request["proto"] = "coretrace-1.0";
        request["id"] = id;
        request["type"] = "request";
        request["method"] = "run_analysis";
        request["params"] = {
            {"input", json::array({(repoRoot / "tests/double_free.c").string()})},
            {"config", (repoRoot / "config/tool-config.json").string()},
            {"invoke", json::array({"ctrace_stack_analyzer"})},
            {"report_file", reportFile.string()},
        };
        return request;
    }

    bool responseIsComplete(const json& response, const std::string& label, TestReport& report)
    {
        const bool ok = response.value("status", "") == "ok";
        report.expect(ok,
                      label + ": status is ok" +
                          (ok ? "" : " (" + response.value("error", json::object()).dump() + ")"));
        if (!ok)
        {
            return false;
        }

        const json& result = response["result"];
        const json& total = result["diagnostics_summary_total"];
        report.expect(total.value("info", -1) == 0 && total.value("warning", -1) == 0 &&
                          total.value("error", -1) == 1,
                      label + ": diagnostics_summary_total is exactly one error");

        const json& outputs = result["outputs"];
        const bool hasToolOutput = outputs.contains("ctrace_stack_analyzer") &&
                                   outputs["ctrace_stack_analyzer"].is_array() &&
                                   !outputs["ctrace_stack_analyzer"].empty();
        report.expect(hasToolOutput, label + ": outputs carry the stack analyzer entries");
        if (!hasToolOutput)
        {
            return false;
        }

        bool foundReport = false;
        for (const json& entry : outputs["ctrace_stack_analyzer"])
        {
            if (entry.value("stream", "") != "stdout" || !entry["message"].is_object())
            {
                continue;
            }
            const json& summary = entry["message"].value("diagnosticsSummary", json::object());
            foundReport = summary.value("error", -1) == 1;
        }
        report.expect(foundReport, label + ": stdout entry is the analyzer JSON report");
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ctrace_server_concurrency_tests <repo-root>\n";
        return 2;
    }
    const std::filesystem::path repoRoot = argv[1];

    coretrace::enable_logging();
    coretrace::set_min_level(coretrace::Level::Warn);
    ctrace::configure_server_logging();

    const auto reportDir = std::filesystem::temp_directory_path() / "ctrace-server-concurrency";
    std::error_code fsError;
    std::filesystem::remove_all(reportDir, fsError);
    std::filesystem::create_directories(reportDir, fsError);

    TestReport report;
    ConsoleLogger logger;
    ApiHandler handler(logger);

    constexpr int kRounds = 3;
    constexpr int kConcurrentRequests = 4;
    for (int round = 0; round < kRounds; ++round)
    {
        std::vector<json> responses(kConcurrentRequests);
        std::vector<std::thread> workers;
        workers.reserve(kConcurrentRequests);
        for (int i = 0; i < kConcurrentRequests; ++i)
        {
            workers.emplace_back(
                [&, i, round]
                {
                    const auto reportFile = reportDir / ("round" + std::to_string(round) + "-req" +
                                                         std::to_string(i) + ".json");
                    responses[static_cast<std::size_t>(i)] =
                        handler.handle_request(makeRequest(repoRoot, reportFile, i));
                });
        }
        for (auto& worker : workers)
        {
            worker.join();
        }

        for (int i = 0; i < kConcurrentRequests; ++i)
        {
            const std::string label =
                "round " + std::to_string(round) + " request " + std::to_string(i);
            const json& response = responses[static_cast<std::size_t>(i)];
            report.expect(response.value("id", -1) == i, label + ": response id matches");
            if (!responseIsComplete(response, label, report))
            {
                continue;
            }
            const auto reportFile = reportDir / ("round" + std::to_string(round) + "-req" +
                                                 std::to_string(i) + ".json");
            std::ifstream in(reportFile);
            const json document = in.is_open() ? json::parse(in, nullptr, false) : json();
            report.expect(
                !document.is_discarded() && document.is_object() &&
                    document.value("diagnosticsSummary", json::object()).value("error", -1) == 1,
                label + ": its own report file holds the same summary");
        }
    }

    if (report.failures == 0)
    {
        std::cout << "server_concurrency_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " server concurrency check(s) failed\n";
    return 1;
}
