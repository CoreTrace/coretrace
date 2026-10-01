// SPDX-License-Identifier: Apache-2.0
//
// mangleFunction builds Itanium C++ ABI symbol names: each result must demangle back to the
// function it describes.
#include "ctrace_tools/mangle.hpp"

#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <memory>
#include <string>
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

    /// The demangled form of `symbol`, or an empty string when it is not a valid symbol.
    std::string demangle(const std::string& symbol)
    {
        int status = 0;
        const std::unique_ptr<char, void (*)(void*)> demangled(
            abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status), std::free);
        return status == 0 ? std::string(demangled.get()) : std::string();
    }
} // namespace

int main()
{
    using ctrace_tools::mangle::mangleFunction;
    TestReport report;

    report.expect(mangleFunction("", "foo", {"void"}) == "_Z3foov",
                  "a free function without parameters");
    report.expect(mangleFunction("", "foo", {"int", "double"}) == "_Z3fooid",
                  "builtin parameter types are encoded in order");
    report.expect(demangle(mangleFunction("", "foo", {"int", "char"})) == "foo(int, char)",
                  "a free function demangles back to its signature");

    if (report.failures == 0)
    {
        std::cout << "mangle_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " mangle check(s) failed\n";
    return 1;
}
