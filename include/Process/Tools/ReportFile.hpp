// SPDX-License-Identifier: Apache-2.0
#ifndef REPORT_FILE_HPP
#define REPORT_FILE_HPP

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace ctrace
{
    /// Writes a report to `reportPath`, creating the parent directory. Returns false with
    /// `errorMessage` set when the file cannot be written; the caller decides how loud to be.
    [[nodiscard]] inline bool writeReportToFile(const std::string& reportPath,
                                                std::string_view content, std::string& errorMessage)
    {
        errorMessage.clear();
        if (reportPath.empty())
        {
            errorMessage = "report path is empty";
            return false;
        }

        try
        {
            const std::filesystem::path targetPath(reportPath);
            const auto parent = targetPath.parent_path();
            if (!parent.empty())
            {
                std::error_code mkdirError;
                std::filesystem::create_directories(parent, mkdirError);
                if (mkdirError)
                {
                    errorMessage = "failed to create report directory '" + parent.string() +
                                   "': " + mkdirError.message();
                    return false;
                }
            }

            std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                errorMessage = "failed to open report file '" + targetPath.string() + "'";
                return false;
            }

            out.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!out.good())
            {
                errorMessage = "failed to write report file '" + targetPath.string() + "'";
                return false;
            }
        }
        catch (const std::exception& ex)
        {
            errorMessage = "failed to write report file '" + reportPath + "': " + ex.what();
            return false;
        }

        return true;
    }
} // namespace ctrace

#endif // REPORT_FILE_HPP
