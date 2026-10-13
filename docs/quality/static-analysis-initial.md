# Static analysis: initial report

CoreTrace analyzed with its own analyzers, plus cppcheck and flawfinder. The final report will
repeat the same protocol on the final commit, so the two can be compared.

| | |
|---|---|
| Date | 2026-10-13 |
| Commit | `f2672ac` (`v0.84.0-72-gf2672ac`, `main`) |
| Scope | The 22 translation units of the `ctrace` target (`src/**/*.cpp`, `main.cpp`) and the project headers they include. Tests and fetched dependencies are excluded. |
| Machine | macOS 15.6.1, Apple M3 (8 cores) |
| `ctrace` | Built from that commit following the README (macOS), Homebrew LLVM/Clang 20.1.2 |
| Tools | cppcheck 2.17.1, flawfinder 2.0.20, stack analyzer v0.22.0, concurrency analyzer v0.7.1 |

## Summary

| Tool | Coverage | Errors | Warnings | Notes |
|---|---|---:|---:|---:|
| cppcheck | 22/22 files | 0 | 18 | 7 |
| flawfinder | 22/22 files | 0 | 1 | 0 |
| stack analyzer | 20/22 files (2 time out, see below) | 0 | 3 | 65 |
| concurrency analyzer | 22/22 files, as one program | 0 | 0 | 0 |
| ikos, tscancode | not run: not installed on the machine | | | |

No finding at error level. Of the 22 warnings, 1 is a minor fix, 4 are false positives (3 of
them already tracked in the stack analyzer), 1 is accepted by design and 16 are style
suggestions. The run also
exposed three defects in the analyzers themselves (last section), which matter more than the
findings.

## Warnings and proposed triage

| Tool | Rule | Location | Proposed triage |
|---|---|---|---|
| cppcheck | `constParameterReference` | `src/App/ToolConfig.cpp:1074` | Fix: `findStackAnalyzerSection` only emits warnings through `ctx`, so it can take a `const&` (the stack analyzer reports it too). |
| cppcheck | `variableScope` | `src/Process/Tools/Sarif.cpp:299` | False positive: `byTool[diagnostic.tool]` must create the tool's entry even when all its reports were merged, so that the tool keeps its run. Narrowing `reports` would drop it. |
| cppcheck | `useStlAlgorithm` (×16) | `Config.cpp:245`, `Files.cpp:29`, `ShippedDefaults.cpp:90,156`, `languageType.cpp:22,31`, `ApiHandler.cpp:67,189,313,374`, `CompileCommands.cpp:250`, `ConcurrencyAnalyzerBridge.cpp:69`, `ConcurrencyAnalyzerToolImplementation.cpp:71`, `Sarif.cpp:66`, `StackAnalyzerToolImplementation.cpp:425,515` | Style suggestion (`std::any_of`, `std::transform`...); to decide case by case. |
| flawfinder | `getenv` (CWE-807, CWE-20) | `src/App/ShippedDefaults.cpp:146` | Accepted: `CT_CLANG` is a documented user override, only tested for emptiness here. |
| stack analyzer | `StackBufferOverflow` (×2) | `src/ctrace_tools/languageType.cpp:19,28` | False positive: the end of a range-based `for` over a local array, tracked in [stack-analyzer#206](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/206). |
| stack analyzer | `StackPointerEscape` | `src/Process/Ipc/GracefulShutdown.cpp:62` | False positive: a temporary passed to the logger through an indirect call, same family as [#182](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/182) and [#209](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/209). |

Notes: cppcheck reports 7 times `normalCheckLevelMaxBranches` (it limited its branch
exploration; the final run should add `--check-level=exhaustive`). The stack analyzer reports 63
`ConstParameterNotModified.Reference` and 2 `ReferenceRvaluePreferValue`, mostly on the `ctx`
parameters of the configuration handlers in `src/App/ToolConfig.cpp`. Those handlers write the
configuration through the `ProgramConfig&` that `LoadContext` holds. A `const LoadContext&`
would compile, because `config` is a reference, but would hide that they modify it.

## Defects found in the analyzers

1. **The concurrency analyzer crashes on macOS when it runs on several jobs.** With the default
   `--jobs`, `ctrace` dies with `SIGBUS`: a stack overflow in Clang's template instantiation
   (`clang::Sema`), on a thread of `ctrace`'s `ThreadPool`. macOS gives secondary threads 512 KiB
   of stack, Linux 8 MiB, so CI does not see it. `-j 1` works (82 s). The cause is in
   coretrace-compiler: `compilerlib::compile`, shared by both analyzers, runs Clang on the calling
   thread without sizing its stack. Fix: run the cc1 job on an 8 MiB thread
   (`clang::DesiredStackSize`).
2. **The pinned stack analyzer (v0.22.0) does not finish on CoreTrace's code.**
   `src/Process/Ipc/ApiHandler.cpp` and `src/Process/Tools/StackAnalyzerToolImplementation.cpp`
   each exceed 300 s, `src/App/Runner.cpp` takes 142 s, and the default whole-program run was
   stopped after 42 minutes. A sample shows the time in `ownership::solve`,
   `AbstractState::join` and `Contents::merge`. The in-process analyzers take no timeout
   (`docs/configuration.md`), so `ctrace` cannot stop them. The stack analyzer's `main`
   (`d7a58d6`, 149 commits after v0.22.0) no longer has the problem: the whole program in 90 s,
   each of the three files in under 10 s. It needs a release, then an update of the pin in
   `cmake/stackUsageAnalyzer.cmake`. The run also uses one core whatever `--jobs`
   ([stack-analyzer#199](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/199)).
3. **A warning in the analyzers' own compilation fails a whole file.** The compilation database
   carries `-Werror`. The analyzers compile with LLVM 20's libc++, which deprecates
   `std::wstring_convert` used by `external/CLI11.hpp`; the build itself uses AppleClang and does
   not warn. `src/App/Config.cpp` then fails to compile for both analyzers, and the stack analyzer
   stops the whole run ([stack-analyzer#200](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/200)).
   Fix: drop `-Werror` from the compile commands the analyzers replay.

`--static` combined with `--invoke` also runs every static tool, ikos included, not only the
listed ones.

## Protocol

Deviations from the defaults, all forced by the defects above: `-Werror` removed from the
compilation database, the stack analyzer run file by file with `--no-resource-cross-tu` and a
300 s limit per file, the concurrency analyzer run with `-j 1`.

```bash
# Build (README, macOS) with a compilation database
L=$(brew --prefix llvm@20)
cmake -S . -B build -G Ninja -DLLVM_DIR="$L/lib/cmake/llvm" -DClang_DIR="$L/lib/cmake/clang" \
  -DCLANG_EXECUTABLE="$L/bin/clang" -DENABLE_PYTHON_ANALYZER=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel 8

# CoreTrace's own translation units, without -Werror
jq '[.[] | select((.file|test("/(src/|main\\.cpp$)")) and ((.output // .command)|contains("CMakeFiles/ctrace.dir")))
     | .command |= gsub(" -Werror"; "")]' build/compile_commands.json > self-cc.json

# cppcheck, flawfinder (pip install flawfinder), concurrency analyzer
for t in cppcheck flawfinder; do
  ./build/ctrace --compile-commands self-cc.json --invoke $t --sarif-format --report-file $t.sarif --fail-on none
done
./build/ctrace --compile-commands self-cc.json --invoke coretrace-concurrency-analyzer -j 1 \
  --sarif-format --report-file concurrency.sarif --fail-on none

# Stack analyzer, one file at a time, 300 s each
echo '{"stack_analyzer": {"extra_args": ["--no-resource-cross-tu"]}}' > sa.json
for i in $(seq 0 $(( $(jq length self-cc.json) - 1 ))); do
  jq ".[$i:$((i+1))]" self-cc.json > cc-$i.json
  perl -e 'alarm shift; exec @ARGV' 300 ./build/ctrace --config sa.json --compile-commands cc-$i.json \
    --invoke ctrace_stack_analyzer -j 1 --sarif-format --report-file sa-$i.sarif --fail-on none
done
```
