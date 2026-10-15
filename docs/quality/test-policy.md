# Test policy

What the tests of `ctrace` must protect, how they are organized, how much of the code they must
cover, and the rules every change follows.

| | |
|---|---|
| Date | 2026-10-13 |
| Commit | `fbee9eb` |
| Measures | The CI coverage report of `main` (llvm-cov), and the CI test runs: 82 tests on Ubuntu 24.04, 80 on macOS 14 |

## The critical path

`ctrace` gates CI pipelines: its exit code is a verdict on the run. The worst failure is a wrong
verdict, above all a pass while findings exist or a tool failed. It has happened before:
external tools whose failure counted as success
([#44](https://github.com/CoreTrace/coretrace/issues/44)), findings left out of the count
([#90](https://github.com/CoreTrace/coretrace/issues/90)), an exit code always 0
([#91](https://github.com/CoreTrace/coretrace/issues/91)).

The critical path is therefore everything between the inputs and the verdict:

| Stage | Files | What can go wrong |
|---|---|---|
| 1. Configuration | `src/App/Config.cpp`, `ToolConfig.cpp`, `ShippedDefaults.cpp`; `include/Config/config.hpp`, `include/App/SupportedTools.hpp`; `src/ctrace_tools/strings.cpp` | A setting ignored or misread |
| 2. Inputs | `src/App/Files.cpp`, `include/App/Files.hpp`, `src/Process/Tools/CompileCommands.cpp`, `include/Process/Tools/InputPaths.hpp`, `src/ctrace_tools/languageType.cpp` | A file not analyzed, or sent to the wrong tool |
| 3. Tool runs | `include/Process/Tools/ToolsInvoker.hpp`, `src/App/Runner.cpp`, `include/Process/UnixProcess.hpp`, `Process.hpp`, `ProcessFactory.hpp`, the tool interfaces and helpers of `include/Process/Tools/`, the seven `*ToolImplementation.cpp`, `ConcurrencyAnalyzerBridge.cpp` | A failed tool counted as a success; output not interpreted |
| 4. Findings | `include/Process/Tools/Diagnostic.hpp`, `src/Process/Tools/Sarif.cpp`, `include/Process/Tools/ReportFile.hpp` | A finding lost, duplicated or not counted |
| 5. Verdict | `include/App/ExitPolicy.hpp`, `main.cpp` | A wrong exit code |

That is 35 files, 4,750 of the 5,518 measured lines (86%).

**The server path** comes second: `src/Process/Ipc/` and `include/Process/Ipc/` (5 files, 699
lines). It returns the same verdict through `result.gate`, and adds request handling.

## Strategy

| Layer | What it checks | When |
|---|---|---|
| Build gates | Compiler warnings as errors, `clang-format`, SPDX license headers, pinned dependencies | Every pull request, as required checks |
| Unit tests (`ctest -L unit`) | One executable per component, in process; plus checks of the release and CI scripts. 19 tests, about 7 s | Every pull request, Ubuntu and macOS |
| Integration tests (`ctest -L integration`) | The real `ctrace` binary, server and socket, run on the fixtures of `tests/`. They use the real tools CI installs (cppcheck, the linked analyzers, the Python and runtime analyzers on Linux), and fake tools (`tests/fake-*.sh`) for failure paths. 63 tests on Ubuntu | Every pull request, Ubuntu and macOS |
| Equivalence | The F7 harness checks that sequential and parallel runs produce the same diagnostic fingerprints | Every pull request, Ubuntu and macOS |
| Release verification | Each Linux archive is unpacked and run, with no compiler, on Rocky Linux 9, Ubuntu 22.04, Debian 12 and Ubuntu 24.04; packaging fails on any unlisted library | Every pull request (the "Build Linux binary" jobs, amd64 and arm64) and every release |
| Coverage | llvm-cov over unit tests, integration tests and both; report published as the `coverage-report` artifact | Every pull request |
| Static analysis | `ctrace` and its analyzers run on CoreTrace's own code ([initial report](static-analysis-initial.md)) | Start and end of the project |

Out of scope here:

- **The precision of the analyzers**, meaning what they find, is tested by their own suites, in
  their repositories. `ctrace` tests that it runs them, reads their reports and gates on them.
- **Windows** is not built ([#54](https://github.com/CoreTrace/coretrace/issues/54)).
- **Speed** is measured by the `-j` benchmark of CI, for information only. No test asserts a
  duration: shared runners cannot make that claim reliably.

## Coverage objective

Baseline, from the CI report of `main`:

| | Tests | Lines | Branches |
|---|---|---:|---:|
| Critical path | all | 90.8% | 82.0% |
| Critical path | unit only | 72.7% | 67.7% |
| Server path | all | 81.1% | 75.0% |
| All of CoreTrace's code | all | 89.6% | 81.5% |

Objectives:

- **Critical path**: never below **90% of lines and 80% of branches** on `main`. Branches go to
  **85% by the end of the project**, about 50 more branches, starting with the least covered
  files: `FlawfinderToolImplementation.cpp` (35.7% of branches),
  `CompileCommands.cpp` (64.5%), `StackAnalyzerToolImplementation.cpp` (66.2%) and
  `UnixProcess.hpp` (73.7%).
- **Server path**: never below 80% of lines and 75% of branches.
- **Whole code**: never below 85% of lines.

There is no separate objective for unit tests. Unit tests check logic inside a component;
integration tests check behavior through the binary. Both count toward the objective.

A copy of the report is kept in the repository, with the share of the critical path: [2026-10-15](coverage/2026-10-15/README.md).

Today the coverage job measures but never fails. Enforcing these floors is the first action
below.

## CI history

Every run of every workflow, with its logs for 90 days:

| Repository | Runs | Required checks of `main` |
|---|---|---:|
| `coretrace` | [Actions](https://github.com/CoreTrace/coretrace/actions) | 7 |
| `coretrace-stack-analyzer` | [Actions](https://github.com/CoreTrace/coretrace-stack-analyzer/actions) | 10 |
| `coretrace-concurrency-analyzer` | [Actions](https://github.com/CoreTrace/coretrace-concurrency-analyzer/actions) | 7 |
| `coretrace-runtime-analyzer` | [Actions](https://github.com/CoreTrace/coretrace-runtime-analyzer/actions) | 4 |
| `coretrace-gui` | [Actions](https://github.com/CoreTrace/coretrace-gui/actions) | 3 |

The failed runs of `coretrace`, and what each one caught, are in
[Regressions caught](regressions.md).

## Team conventions

1. **A change of behavior comes with its test, in the same pull request.** A bug fix comes with
   a test that fails without the fix. For example,
   [#150](https://github.com/CoreTrace/coretrace/pull/150) added the mangling tests with its fix
   for [#144](https://github.com/CoreTrace/coretrace/issues/144), and the string-slice test of
   [#166](https://github.com/CoreTrace/coretrace/pull/166) fails on the code before the fix.
2. **Unit tests** go in `tests/<component>_tests.cpp`, as an executable named
   `ctrace_<component>_tests`. They follow the `TestReport` and `expect` pattern of
   `tests/tool_output_tests.cpp`, and must be added to `coretrace_unit_tests` in
   `CMakeLists.txt`; any test not in that list is labelled `integration`.
3. **Integration tests** are named after the behavior they check, for example
   `ctrace_exit_3_when_a_tool_cannot_run`. They use fixtures in `tests/`, and fake tools for
   failure paths.
4. **Unit tests use no network and no external tool**, and their result must not depend on
   timing or ordering. Diagnostics are reported in a fixed order
   ([#135](https://github.com/CoreTrace/coretrace/issues/135)), so outputs can be compared.
5. **Retries are for network steps only** (`scripts/ci/retry.sh`). A test that fails
   intermittently is a bug to fix, not to retry.
6. **Before opening a pull request**, run `ctest --test-dir build --output-on-failure` and
   `./scripts/format-check.sh`, as `CONTRIBUTING.md` says. Check the `coverage-report`
   artifact for the files you changed.
7. **A pull request merges only when the seven required checks pass**. The `main` ruleset
   enforces it.

## Actions

1. Make `scripts/ci/coverage.sh` fail when the critical path or the server path falls below its
   floor.
2. Add tests for the four least covered files of the critical path.
3. Cover `RunDirectory`: [#164](https://github.com/CoreTrace/coretrace/issues/164), a welcome
   ticket.
4. Test the timeout path of the linked analyzers along with
   [#172](https://github.com/CoreTrace/coretrace/issues/172).
