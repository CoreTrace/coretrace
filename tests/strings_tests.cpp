// SPDX-License-Identifier: Apache-2.0
//
// splitByComma parses comma-separated values from the CLI, the configuration and HTTP
// request fields, so it must be well defined on any input.
#include "ctrace_tools/strings.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

    bool splitsInto(std::string_view input, const std::vector<std::string_view>& expected)
    {
        return ctrace_tools::strings::splitByComma(input) == expected;
    }
} // namespace

int main()
{
    TestReport report;

    report.expect(splitsInto("a,b,c", {"a", "b", "c"}), "plain tokens are split on commas");
    report.expect(splitsInto(" a , b ,c ", {"a", "b", "c"}), "spaces around tokens are trimmed");
    report.expect(splitsInto("a,,b,", {"a", "b"}), "empty tokens are dropped");
    report.expect(splitsInto("", {}), "an empty input gives no token");
    report.expect(splitsInto("\"a,b\",c", {"a,b", "c"}), "a comma inside quotes does not split");

    // A lone quote is not a quoted token: unquoting it would read past an empty view.
    report.expect(splitsInto("a,\"", {"a", "\""}), "a trailing lone quote is kept as is");
    report.expect(splitsInto("\"", {"\""}), "a lone quote is kept as is");
    report.expect(splitsInto("\"\"", {}), "an empty quoted token is dropped");

    // Spaces are trimmed before unquoting; spaces inside the quotes belong to the value.
    report.expect(splitsInto("a, \"b c\" ,d", {"a", "b c", "d"}),
                  "quotes are removed from a token surrounded by spaces");
    report.expect(splitsInto(" \" b \" ", {" b "}), "spaces inside quotes are kept");

    if (report.failures == 0)
    {
        std::cout << "strings_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " strings check(s) failed\n";
    return 1;
}
