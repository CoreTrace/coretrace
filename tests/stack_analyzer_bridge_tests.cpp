// SPDX-License-Identifier: Apache-2.0
//
// Compatibility gate between coretrace and the pinned coretrace-stack-analyzer library.
// Runs the real bridge on a fixture with the canonical configuration and checks that the
// structured summary coretrace exposes matches the analyzer's own report.
#include "App/ToolConfig.hpp"
#include "Process/Tools/AnalysisTools.hpp"
#include "app/AnalyzerApp.hpp"

#include <coretrace/logger.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

// The bridge reads AnalysisReport fields by name; a contract bump means the bridge must be
// reviewed before the analyzer pin is moved.
static_assert(ctrace::stack::app::kAnalysisReportContractVersion == 1,
              "coretrace consumes AnalysisReport contract v1; review the stack analyzer bridge");

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

    std::filesystem::path makeReportPath()
    {
        const auto dir = std::filesystem::temp_directory_path() / "ctrace-bridge-tests";
        std::error_code err;
        std::filesystem::create_directories(dir, err);
        const auto path = dir / "double_free-report.json";
        std::filesystem::remove(path, err);
        return path;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ctrace_stack_analyzer_bridge_tests <repo-root>\n";
        return 2;
    }
    const std::filesystem::path repoRoot = argv[1];

    coretrace::enable_logging();
    coretrace::set_min_level(coretrace::Level::Error);

    TestReport report;

    ctrace::ProgramConfig config;
    std::string configError;
    const bool configLoaded = ctrace::applyToolConfigFile(
        config, (repoRoot / "config/tool-config.json").string(), configError);
    report.expect(configLoaded, "canonical config/tool-config.json loads: " + configError);
    if (!configLoaded)
    {
        return 1;
    }

    const std::filesystem::path reportPath = makeReportPath();
    config.output.report_file = reportPath.string();
    const std::string input = (repoRoot / "tests/double_free.c").string();

    ctrace::StackAnalyzerToolImplementation tool;
    tool.executeBatch(std::vector<std::string>{input}, config);
    const ctrace::DiagnosticSummary summary = tool.lastDiagnosticsSummary();

    report.expect(summary.info == 0 && summary.warning == 0 && summary.error == 1,
                  "double_free.c yields exactly one error-level diagnostic (got info=" +
                      std::to_string(summary.info) +
                      ", warning=" + std::to_string(summary.warning) +
                      ", error=" + std::to_string(summary.error) + ")");

    std::ifstream reportStream(reportPath);
    report.expect(reportStream.is_open(), "report file is persisted at output.report_file");
    if (reportStream.is_open())
    {
        const nlohmann::json document = nlohmann::json::parse(reportStream, nullptr, false);
        const bool isAnalyzerJson = !document.is_discarded() && document.is_object() &&
                                    document.contains("diagnostics") &&
                                    document.contains("diagnosticsSummary");
        report.expect(isAnalyzerJson, "report file is the analyzer JSON document");
        if (isAnalyzerJson)
        {
            const auto& fileSummary = document["diagnosticsSummary"];
            report.expect(fileSummary.value("info", -1) == static_cast<int>(summary.info) &&
                              fileSummary.value("warning", -1) ==
                                  static_cast<int>(summary.warning) &&
                              fileSummary.value("error", -1) == static_cast<int>(summary.error),
                          "bridge summary equals the report's diagnosticsSummary");
            const auto& diagnostics = document["diagnostics"];
            report.expect(diagnostics.is_array() && diagnostics.size() == 1 &&
                              diagnostics[0].value("ruleId", "") ==
                                  "ResourceLifetime.DoubleRelease",
                          "the diagnostic is ResourceLifetime.DoubleRelease");
        }
    }

    if (report.failures == 0)
    {
        std::cout << "stack_analyzer_bridge_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " stack analyzer bridge check(s) failed\n";
    return 1;
}
