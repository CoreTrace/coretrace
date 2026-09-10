// SPDX-License-Identifier: Apache-2.0
//
// How the analysis tools are invoked and resolved.

#include "App/ToolResolver.hpp"
#include "Process/Tools/CppCheckArguments.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int failures = 0;

    void check(bool condition, const std::string& what)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << what << "\n";
            ++failures;
            return;
        }
        std::cerr << "ok: " << what << "\n";
    }

    bool contains(const std::vector<std::string>& values, const std::string& needle)
    {
        return std::find(values.begin(), values.end(), needle) != values.end();
    }

    void testCppcheckAsksForTheChecksItCanRun()
    {
        // With no --enable list cppcheck reports errors only and stays quiet
        // about buffer overruns and dead stores it can see perfectly well.
        const auto arguments = ctrace::cppcheckArguments(true, "a.c");
        check(contains(arguments, "--enable=warning,style,performance,portability"),
              "cppcheck is asked for warning, style, performance and portability checks");
        check(!contains(arguments, "--enable=all"),
              "but not for information, whose missingIncludeSystem notice per header is noise");
        check(contains(arguments, "--output-format=sarif"), "SARIF is requested when asked for");
        check(!arguments.empty() && arguments.back() == "a.c", "the file comes last");

        const auto plain = ctrace::cppcheckArguments(false, "a.c");
        check(!contains(plain, "--output-format=sarif"), "and not otherwise");
    }

    void testFlawfinderIsNotLookedForInsideTheProject()
    {
        // The bundled script used to be named "./flawfinder/..." — relative to
        // the current directory, which during a run is the project under
        // analysis, so flawfinder was missing from every workspace that did not
        // contain a copy of it.
        const auto command = ctrace::resolveFlawfinderCommand();
        for (const auto& argument : command.prefixArguments)
        {
            check(argument.rfind("./", 0) != 0 && argument.rfind(".\\", 0) != 0,
                  "no argument is resolved against the current directory: " + argument);
        }
        const bool viaModule = contains(command.prefixArguments, "-m") &&
                               contains(command.prefixArguments, "flawfinder");
        const bool viaBundledScript =
            !command.prefixArguments.empty() &&
            command.prefixArguments.front().find("flawfinder.py") != std::string::npos;
        check(viaModule || viaBundledScript,
              "flawfinder is run from the copy shipped with ctrace or as an installed module");
    }
} // namespace

int main()
{
    testCppcheckAsksForTheChecksItCanRun();
    testFlawfinderIsNotLookedForInsideTheProject();
    return failures == 0 ? 0 : 1;
}
