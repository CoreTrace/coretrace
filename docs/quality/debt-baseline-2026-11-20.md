# Debt baseline: cppcheck and clang-tidy

A measure of `ctrace`'s own code with two external analyzers, to count findings by severity and
name the files that concentrate them. It is the "measure" step of the debt cycle of
[AGENTS.md](../../AGENTS.md): the same commands on a later commit give the trend.

| | |
|---|---|
| Date | 2026-11-20 |
| Commit | `00965c9` (branch `docs/agents-md`; the code is that of `main` `7de854e`) |
| Scope | The 22 translation units of the `ctrace` target (`src/**/*.cpp`, `main.cpp`) and the project headers they include (`include/`). Tests, `external/` and fetched dependencies are excluded. |
| Machine | macOS 27.0.1, Apple M3 (8 cores) |
| Tools | cppcheck 2.17.1, clang-tidy 20.1.2 (Homebrew LLVM) |
| Not run | The stack and concurrency analyzers: they need a built `ctrace`. See [Limits](#limits). |

## Summary

| Tool | Files | Errors | Warnings | Style | Information |
|---|---|---:|---:|---:|---:|
| cppcheck | 22/22 | 0 | 0 | 18 | 8 |
| clang-tidy | 22/22, no compilation error | 0 | 51 | — | — |

69 findings in 23 files, none at error level. clang-tidy reports every check as a warning: its
families, below, are the useful split.

| clang-tidy family | Findings | Checks |
|---|---:|---|
| `concurrency` | 16 | `mt-unsafe` ×16 |
| `performance` | 14 | `enum-size` ×6, `no-automatic-move` ×3, `faster-string-find` ×2, `inefficient-string-concatenation` ×2, `avoid-endl` ×1 |
| `bugprone` | 11 | `easily-swappable-parameters` ×9, `exception-escape` ×1, `nondeterministic-pointer-iteration-order` ×1 |
| `cert` | 5 | `err33-c` ×4, `err58-cpp` ×1 |
| `readability` (complexity) | 4 | `function-cognitive-complexity` ×4 |
| `clang-analyzer` | 1 | `optin.performance.Padding` ×1 |

cppcheck: `useStlAlgorithm` ×17 and `variableScope` ×1 (style);
`normalCheckLevelMaxBranches` ×8 (information: it limited its branch exploration).

Against the [initial report](static-analysis-initial.md) of 2026-10-13, for cppcheck, the only
tool both runs share: 18 findings then, 18 now. The `constParameterReference` of
`ToolConfig.cpp` is fixed ([#177](https://github.com/CoreTrace/coretrace/issues/177)), and one
more raw loop is flagged in `ApiHandler.cpp` (5 instead of 4).

## Hotspots

Findings per file, cppcheck's information notes left out.

| | File | Findings | clang-tidy | cppcheck | Lines | What |
|---|---|---:|---:|---:|---:|---|
| 1 | `include/Process/UnixProcess.hpp` | 9 | 9 | 0 | 343 | `strerror` ×8 and `getenv` ×1, not thread-safe |
| 2 | `src/Process/Ipc/ApiHandler.cpp` | 6 | 1 | 5 | 543 | 5 raw loops; `run_analysis` has a cognitive complexity of 35 |
| 3 | `src/Process/Tools/StackAnalyzerToolImplementation.cpp` | 6 | 4 | 2 | 627 | `buildAnalyzerArgs` has a cognitive complexity of 68; swappable parameters ×3 |
| 4 | `include/Process/Interrupt.hpp` | 5 | 5 | 0 | 97 | Return values of `std::signal` and `std::raise` ignored ×4 |
| 5 | `src/App/ShippedDefaults.cpp` | 5 | 3 | 2 | 162 | `getenv` and `setenv`; 2 raw loops |

Then `IpcStrategy.hpp`, `Sarif.cpp` and `ToolConfig.cpp`, with 4 each.

Functions above clang-tidy's cognitive complexity threshold (25):

| Function | File | Complexity |
|---|---|---:|
| `applyToolsSection` | `src/App/ToolConfig.cpp:1140` | 100 |
| `buildAnalyzerArgs` | `src/Process/Tools/StackAnalyzerToolImplementation.cpp:96` | 68 |
| `run_analysis` | `src/Process/Ipc/ApiHandler.cpp:294` | 35 |
| `splitCommandLine` | `src/Process/Tools/CompileCommands.cpp:22` | 28 |

## Proposed triage

None of these is recorded as debt yet: each line below is a proposal for the maintainer, who
opens the issue.

| Finding | Location | Proposal |
|---|---|---|
| `concurrency-mt-unsafe`: `strerror` ×13 | `UnixProcess.hpp`, `IpcStrategy.hpp`, `RunDirectory.hpp` | To fix: tools run on a thread pool, and two failures at the same time can mix their error text. One helper over `strerror_r` or `std::system_category().message()` removes all 13. |
| `concurrency-mt-unsafe`: `getenv` ×2, `setenv` ×1 | `UnixProcess.hpp:287`, `ShippedDefaults.cpp:146,157` | `setenv("CT_CLANG")` is accepted: `main.cpp` calls it before any thread starts. The `getenv("PATH")` of `UnixProcess.hpp` runs on pool threads: to check. |
| `bugprone-exception-escape` | `src/ctrace_tools/strings.cpp:32` | To check first: `splitByComma` is `noexcept` and fills a `std::vector`, so an allocation failure ends the process. It is on the configuration stage of the critical path. |
| `cert-err33-c` ×4 | `include/Process/Interrupt.hpp:35,36,91,92` | Probably accepted: `std::signal` and `std::raise` on the way out of the process, where nothing can handle a failure. Say so with a `NOLINT` and its reason. |
| `function-cognitive-complexity` ×4 | table above | `applyToolsSection` (100) and `buildAnalyzerArgs` (68) are the two to split; both are on the critical path. |
| `bugprone-nondeterministic-pointer-iteration-order` | `include/Process/Tools/ToolsInvoker.hpp:411` | Probably a false positive: a `std::stable_partition` on a flag of each queue, not an order on pointer values. To confirm, since a deterministic output is an invariant. |
| `bugprone-easily-swappable-parameters` ×9, `performance-*` ×14, `Padding` ×1, `err58-cpp` ×1 | several | Style and micro-optimizations: decide per check whether it joins the gate, rather than per finding. |
| cppcheck `useStlAlgorithm` ×17, `variableScope` ×1 | several | Already known: D11 of the [register](technical-debt.md), and a false positive of the initial report. |

## Limits

- **Two tools out of four.** The stack and concurrency analyzers were not run: this measure
  does not replace the final static analysis.
- **`ctrace` only.** No analyzer repository was measured.
- **The check set is a choice.** The repository has no `.clang-tidy`; the families below were
  picked for this measure. Another set gives other counts, so only runs with the same set
  compare.
- **cppcheck sees no include path**, as when `ctrace` runs it: it checks each `.cpp` without
  the project headers. clang-tidy compiles each unit with its real command, and a finding in a
  header is counted once, whatever the number of units that include it.
- **A count is a proxy.** 69 findings say where to look, not how much the debt costs; the
  triage above already sets some aside.

## Protocol

```bash
# Compilation database: configure only, nothing is built
L=$(brew --prefix llvm@20)
cmake -S . -B build -G Ninja -DLLVM_DIR="$L/lib/cmake/llvm" -DClang_DIR="$L/lib/cmake/clang" \
  -DCLANG_EXECUTABLE="$L/bin/clang" -DENABLE_PYTHON_ANALYZER=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# CoreTrace's own translation units, without -Werror, as in the initial report
mkdir -p self
jq '[.[] | select((.file|test("/(src/|main\\.cpp$)")) and ((.output // .command)|contains("CMakeFiles/ctrace.dir")))
     | .command |= gsub(" -Werror"; "")]' build/compile_commands.json > self/compile_commands.json
jq -r '.[].file' self/compile_commands.json | sed "s|^$PWD/||" | sort > self/files.txt

# cppcheck, with the options ctrace gives it
cppcheck --template='{file}|{line}|{severity}|{id}|{message}' \
  --enable=warning,style,performance,portability --inline-suppr --quiet \
  --file-list=self/files.txt 2> cppcheck.txt

# clang-tidy, two files at a time (about 90 s)
CHECKS='-*,bugprone-*,clang-analyzer-*,cert-*,concurrency-*,performance-*,portability-*,readability-function-cognitive-complexity,readability-function-size'
xargs -P 2 -n 1 "$L/bin/clang-tidy" -p self --checks="$CHECKS" \
  --header-filter="^$PWD/(src|include)/" --quiet < self/files.txt > clang-tidy.txt
```

Counting: one clang-tidy finding per (normalized file, line, column, check), kept when the file
is under `src/`, `include/` or is `main.cpp`. The raw output holds 310 lines for 51 findings,
because a header is reported once per unit that includes it.
