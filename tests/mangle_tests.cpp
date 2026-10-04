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
    using ctrace_tools::mangle::isMangled;
    using ctrace_tools::mangle::mangleFunction;
    TestReport report;

    report.expect(mangleFunction("", "foo", {"void"}) == "_Z3foov",
                  "a free function without parameters");
    report.expect(mangleFunction("", "foo", {"int", "double"}) == "_Z3fooid",
                  "builtin parameter types are encoded in order");
    report.expect(demangle(mangleFunction("", "foo", {"int", "char"})) == "foo(int, char)",
                  "a free function demangles back to its signature");

    // The nested name N...E holds the qualified name only; the parameters follow it.
    report.expect(mangleFunction("ns", "func", {"void"}) == "_ZN2ns4funcEv",
                  "a namespaced function closes its nested name before the parameters");
    report.expect(demangle(mangleFunction("ns", "func", {"int"})) == "ns::func(int)",
                  "a namespaced function demangles back to its signature");

    report.expect(isMangled("_Z4mainv"), "_Z4mainv is recognized as mangled");
    report.expect(!isMangled("main"), "unmangled 'main' returns false");
    report.expect(!isMangled(""), "empty string returns false");
    report.expect(!isMangled("_"), "single underscore returns false");
    report.expect(!isMangled("_Zgarbage"), "invalid mangled symbol returns false");

    // The view stops before "XYZ", but its buffer does not: isMangled must read only the
    // view, since "_Z3foovXYZ" as a whole does not demangle.
    const std::string_view symbolFollowedByGarbage("_Z3foovXYZ");
    report.expect(isMangled(symbolFollowedByGarbage.substr(0, 7)),
                  "std::string_view slice works correctly with isMangled");

    report.expect(mangleFunction("", "f", {"int"}) == "_Z1fi", "int encodes as i");
    report.expect(mangleFunction("", "f", {"double"}) == "_Z1fd", "double encodes as d");
    report.expect(mangleFunction("", "f", {"char"}) == "_Z1fc", "char encodes as c");
    report.expect(mangleFunction("", "f", {"float"}) == "_Z1ff", "float encodes as f");
    report.expect(mangleFunction("", "f", {"bool"}) == "_Z1fb", "bool encodes as b");
    report.expect(mangleFunction("", "f", {"CustomType"}) == "_Z1f10CustomType",
                  "unknown type encodes as length + name");

    const std::string mangled = mangleFunction("", "foo", {"int", "double"});
    report.expect(isMangled(mangled),
                  "round trip: mangleFunction output is recognized by isMangled");

    if (report.failures == 0)
    {
        std::cout << "mangle_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " mangle check(s) failed\n";
    return 1;
}
