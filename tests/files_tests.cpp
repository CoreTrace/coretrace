// SPDX-License-Identifier: Apache-2.0
//
// Source file resolution: plain paths and compile_commands.json, nothing else.
#include "App/Files.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
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

    std::filesystem::path scratchDir()
    {
        const auto dir = std::filesystem::temp_directory_path() / "ctrace-files-tests";
        std::error_code err;
        std::filesystem::remove_all(dir, err);
        std::filesystem::create_directories(dir, err);
        return dir;
    }

    void writeFile(const std::filesystem::path& path, const std::string& content)
    {
        std::ofstream out(path);
        out << content;
    }

    std::string norm(const std::filesystem::path& p)
    {
        return p.lexically_normal().string();
    }

    ctrace::SourceFileResolution resolve(const std::vector<std::string>& input,
                                         const std::string& compileCommands = "",
                                         bool includeDeps = false)
    {
        ctrace::ProgramConfig config;
        config.files.input = input;
        config.files.compile_commands = compileCommands;
        config.files.include_compdb_deps = includeDeps;
        return ctrace::resolveSourceFiles(config);
    }
} // namespace

int main()
{
    TestReport report;
    const auto dir = scratchDir();
    // An input must exist: the plain paths below are files of the scratch directory, which the
    // relative ones are relative to.
    std::filesystem::current_path(dir);
    for (const char* file : {"a.c", "b.c", "-F.c", "main.c", "src/-t.c", "-x.c", "extra.c",
                             "src/x.c", "abs/dir/y.c", "build/_deps/lib/z.c", "already/absolute.c"})
    {
        std::filesystem::create_directories((dir / file).parent_path());
        writeFile(dir / file, "");
    }
    const std::string absoluteDash = (dir / "-x.c").string();

    // Plain paths: normalized and deduplicated, order preserved.
    {
        const auto r = resolve({"./a.c", "b.c", "a.c", ""});
        report.expect(r.ok(), "plain paths: resolves without error");
        report.expect((r.files == std::vector<std::string>{"a.c", "b.c"}),
                      "plain paths: normalized and deduplicated");
    }

    // A name starting with `-` keeps a `./` in front, or the tools read it as an option; both
    // spellings are one file. Other paths are left as they are.
    {
        const auto r = resolve({"-F.c", "./-F.c", "main.c", "src/-t.c", absoluteDash});
        report.expect(
            (r.files == std::vector<std::string>{"./-F.c", "main.c", "src/-t.c", absoluteDash}),
            "dash paths: './' kept in front of a leading '-', both spellings deduplicated");
    }

    // An explicit compile database: entries resolve against their directory (relative to the
    // database location) and _deps entries are kept because the user named the file.
    const auto compdb = dir / "compile_commands.json";
    const std::string absoluteDir = norm(dir / "abs/dir");
    const std::string absoluteFile = norm(dir / "already/absolute.c");
    writeFile(compdb, R"json([
  {"directory": "src", "file": "x.c", "command": "cc x.c"},
  {"directory": ")json" + absoluteDir +
                          R"json(", "file": "y.c", "command": "cc y.c"},
  {"directory": "build/_deps/lib", "file": "z.c", "command": "cc z.c"},
  {"file": ")json" + absoluteFile +
                          R"json(", "command": "cc absolute.c"}
])json");
    {
        const auto r = resolve({compdb.string()});
        report.expect(r.ok(), "explicit compdb: resolves without error (" + r.error + ")");
        report.expect(
            (r.files == std::vector<std::string>{norm(dir / "src/x.c"), norm(dir / "abs/dir/y.c"),
                                                 norm(dir / "build/_deps/lib/z.c"), absoluteFile}),
            "explicit compdb: entries resolve against their directory, _deps kept");
    }

    // Auto-discovery from files.compile_commands (directory form): _deps filtered unless asked.
    {
        const auto r = resolve({}, dir.string());
        report.expect(r.ok() && (r.files == std::vector<std::string>{norm(dir / "src/x.c"),
                                                                     norm(dir / "abs/dir/y.c"),
                                                                     absoluteFile}),
                      "auto-discovered compdb: _deps entries are filtered by default");
        const auto kept = resolve({}, dir.string(), /*includeDeps=*/true);
        report.expect(kept.ok() && kept.files.size() == 4,
                      "auto-discovered compdb: include_compdb_deps keeps _deps entries");
    }

    // Anything else that ends in .json is an error, with the path in the message.
    const auto manifest = dir / "manifest.json";
    writeFile(manifest, R"json({"files": ["a.c", "b.c"]})json");
    {
        const auto r = resolve({manifest.string()});
        report.expect(!r.ok() && r.error.find(manifest.string()) != std::string::npos &&
                          r.error.find("compile_commands.json") != std::string::npos,
                      "object manifest: rejected with an explicit error (" + r.error + ")");
    }
    const auto list = dir / "list.json";
    writeFile(list, R"json(["a.c", "b.c"])json");
    {
        const auto r = resolve({list.string()});
        report.expect(!r.ok() && r.error.find(list.string()) != std::string::npos,
                      "array-of-strings manifest: rejected with an explicit error");
    }
    const auto broken = dir / "broken.json";
    writeFile(broken, "{ not json");
    {
        const auto r = resolve({broken.string()});
        report.expect(!r.ok() && r.error.find(broken.string()) != std::string::npos,
                      "invalid JSON: rejected with an explicit error (was silently skipped)");
    }
    {
        const auto missing = (dir / "nope.json").string();
        const auto r = resolve({missing});
        report.expect(!r.ok() && r.error.find(missing) != std::string::npos,
                      "missing .json input: rejected with an explicit error");
    }
    {
        const auto r = resolve({compdb.string(), "extra.c"});
        report.expect(r.ok() && r.files.size() == 5 && r.files.back() == "extra.c",
                      "mixed input: compdb entries then plain paths");
    }

    // A missing input, or a missing compile database, is an error that names it.
    {
        const auto r = resolve({"a.c", "no_such_input.c"});
        report.expect(!r.ok() && r.error.find("no_such_input.c") != std::string::npos,
                      "missing input: rejected with its path (" + r.error + ")");
        const auto database = resolve({}, (dir / "no/such/build").string());
        report.expect(!database.ok() && database.error.find("no/such/build") != std::string::npos,
                      "missing compile database: rejected with its path (" + database.error + ")");
    }

    // The database is checked even when inputs are given, and so is every source it lists that
    // is kept: one the _deps filter leaves out is not.
    {
        const auto r = resolve({"a.c"}, (dir / "no/such/build").string());
        report.expect(!r.ok() && r.error.find("no/such/build") != std::string::npos,
                      "missing compile database beside an input: rejected with its path (" +
                          r.error + ")");
    }
    std::filesystem::create_directories(dir / "missing-source");
    const auto listsMissing = dir / "missing-source/compile_commands.json";
    writeFile(listsMissing, R"json([
  {"directory": ".", "file": "gone.c", "command": "cc gone.c"}
])json");
    {
        const auto r = resolve({listsMissing.string()});
        report.expect(!r.ok() &&
                          r.error.find(norm(dir / "missing-source/gone.c")) != std::string::npos &&
                          r.error.find(listsMissing.string()) != std::string::npos,
                      "compile database listing a missing source: rejected with both paths (" +
                          r.error + ")");
    }
    std::filesystem::create_directories(dir / "missing-dependency");
    writeFile(dir / "missing-dependency/main.c", "");
    writeFile(dir / "missing-dependency/compile_commands.json", R"json([
  {"directory": ".", "file": "main.c", "command": "cc main.c"},
  {"directory": ".", "file": "_deps/lib/gone.c", "command": "cc gone.c"}
])json");
    {
        const auto r = resolve({}, (dir / "missing-dependency").string());
        report.expect(r.ok() && r.files.size() == 1,
                      "missing source filtered out as a dependency: not an error (" + r.error +
                          ")");
    }

    if (report.failures == 0)
    {
        std::cout << "files_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " files check(s) failed\n";
    return 1;
}
