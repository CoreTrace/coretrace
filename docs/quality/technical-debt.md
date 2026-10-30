# Technical debt

The debt of `ctrace`: what makes each change slower or riskier than it should be, how it is
tracked, what has been repaid, and what is left.

| | |
|---|---|
| Date | 2026-10-13 |
| Commit | `a4aece8` |
| Sources | Issues labelled `severity/*` or `refactor`; the [initial static analysis](static-analysis-initial.md); the CI coverage report; a review of the build files, the deprecated options and the documentation |

## How debt is tracked

Each debt item is an issue in this repository. The issue states the problem, the fix and its
acceptance criteria, and the pull request that repays it closes it. Its label gives the
severity:

| Label | Meaning |
|---|---|
| `severity/critical` | Blocks reliable use; fix first |
| `severity/major` | Structural debt; costs every change |
| `severity/minor` | Bug or leftover; do not reproduce |
| `refactor` | Architecture change |

## Repaid

Three reviews recorded 36 issues. 35 are closed.

| Review | Issues | Critical | Major | Minor | Other | Closed |
|---|---|---:|---:|---:|---:|---|
| Audit, 2026-09-14 | [#41](https://github.com/CoreTrace/coretrace/issues/41)–[#65](https://github.com/CoreTrace/coretrace/issues/65) | 4 | 11 | 9 | 1 | 24, between 2026-09-19 and 2026-09-27; [#54](https://github.com/CoreTrace/coretrace/issues/54) is still open |
| Review, 2026-09-21 | [#89](https://github.com/CoreTrace/coretrace/issues/89)–[#93](https://github.com/CoreTrace/coretrace/issues/93) | 2 | 2 | 1 | 0 | all 5, the next day |
| Review, 2026-10-01 to 10-04 | [#133](https://github.com/CoreTrace/coretrace/issues/133), [#136](https://github.com/CoreTrace/coretrace/issues/136), [#142](https://github.com/CoreTrace/coretrace/issues/142)–[#144](https://github.com/CoreTrace/coretrace/issues/144), [#167](https://github.com/CoreTrace/coretrace/issues/167) | 0 | 1 | 3 | 2 | all 6, within 4 days |

The main repayments:

- **Analyzer output.** Results were scraped with regexes from text captured by `dup2` on the
  process-wide descriptors. They now come from a structured report ([#41](https://github.com/CoreTrace/coretrace/issues/41), [#42](https://github.com/CoreTrace/coretrace/issues/42) → [#66](https://github.com/CoreTrace/coretrace/issues/66)), which also
  removed the races of server mode ([#43](https://github.com/CoreTrace/coretrace/issues/43) → [#69](https://github.com/CoreTrace/coretrace/issues/69)).
- **The CI gate.** The exit status of external tools was never checked, and `ctrace` always
  exited with 0 ([#44](https://github.com/CoreTrace/coretrace/issues/44) → [#67](https://github.com/CoreTrace/coretrace/issues/67), [#91](https://github.com/CoreTrace/coretrace/issues/91) → [#96](https://github.com/CoreTrace/coretrace/issues/96)). It now fails a pipeline on findings or on a broken tool.
- **Configuration.** Three sources of truth, a 63-field structure, 12 calls to `std::exit` and a
  five-layer argument parser ([#45](https://github.com/CoreTrace/coretrace/issues/45)–[#49](https://github.com/CoreTrace/coretrace/issues/49)) became one schema-driven loader ([#73](https://github.com/CoreTrace/coretrace/issues/73)–[#78](https://github.com/CoreTrace/coretrace/issues/78)).
- **Build and tests.** Warnings were disabled and CI ran no tests ([#55](https://github.com/CoreTrace/coretrace/issues/55)). CI now builds with
  `-Werror` and runs `ctest` and coverage on every pull request.
- **One output channel** instead of three ([#51](https://github.com/CoreTrace/coretrace/issues/51) → [#79](https://github.com/CoreTrace/coretrace/issues/79)). The server is hardened: bounded request
  bodies, CORS and the shutdown token ([#61](https://github.com/CoreTrace/coretrace/issues/61) → [#85](https://github.com/CoreTrace/coretrace/issues/85)).

<details>
<summary>All 36 issues</summary>

| Issue | Severity | Problem | Repaid by | Closed |
|---|---|---|---|---|
| [#41](https://github.com/CoreTrace/coretrace/issues/41) | critical | Stack analyzer results scraped from text with regexes | [#66](https://github.com/CoreTrace/coretrace/issues/66) | 2026-09-19 |
| [#42](https://github.com/CoreTrace/coretrace/issues/42) | critical | Analyzer output captured by `dup2` on process-wide descriptors | [#66](https://github.com/CoreTrace/coretrace/issues/66) | 2026-09-19 |
| [#43](https://github.com/CoreTrace/coretrace/issues/43) | critical | Concurrent `/api` requests race on fd capture | [#69](https://github.com/CoreTrace/coretrace/issues/69) | 2026-09-19 |
| [#44](https://github.com/CoreTrace/coretrace/issues/44) | critical | External tool exit status never checked | [#67](https://github.com/CoreTrace/coretrace/issues/67) | 2026-09-19 |
| [#45](https://github.com/CoreTrace/coretrace/issues/45) | major | Three parallel sources of truth for the configuration | [#78](https://github.com/CoreTrace/coretrace/issues/78) | 2026-09-19 |
| [#46](https://github.com/CoreTrace/coretrace/issues/46) | major | 63-field flat `GlobalConfig` | [#75](https://github.com/CoreTrace/coretrace/issues/75) | 2026-09-19 |
| [#47](https://github.com/CoreTrace/coretrace/issues/47) | major | `std::exit` called from 12 sites | [#74](https://github.com/CoreTrace/coretrace/issues/74) | 2026-09-19 |
| [#48](https://github.com/CoreTrace/coretrace/issues/48) | major | Five-layer argument parser hiding CLI11 | [#73](https://github.com/CoreTrace/coretrace/issues/73) | 2026-09-19 |
| [#49](https://github.com/CoreTrace/coretrace/issues/49) | major | ODR violation in `CLI11ArgumentParser.cpp` | [#73](https://github.com/CoreTrace/coretrace/issues/73) | 2026-09-19 |
| [#50](https://github.com/CoreTrace/coretrace/issues/50) | major | External tool paths hard-coded relative to the build directory | [#84](https://github.com/CoreTrace/coretrace/issues/84) | 2026-09-21 |
| [#51](https://github.com/CoreTrace/coretrace/issues/51) | major | Three parallel output channels | [#79](https://github.com/CoreTrace/coretrace/issues/79) | 2026-09-19 |
| [#52](https://github.com/CoreTrace/coretrace/issues/52) | major | IPC socket mode applied inconsistently | [#83](https://github.com/CoreTrace/coretrace/issues/83) | 2026-09-20 |
| [#53](https://github.com/CoreTrace/coretrace/issues/53) | major | `--dyn` runs three placeholder tools | [#120](https://github.com/CoreTrace/coretrace/issues/120), through [#112](https://github.com/CoreTrace/coretrace/issues/112) | 2026-09-27 |
| [#54](https://github.com/CoreTrace/coretrace/issues/54) | major | Windows backend does not compile; port reverted | — | open |
| [#55](https://github.com/CoreTrace/coretrace/issues/55) | major | Warnings disabled; CI runs no tests | [#82](https://github.com/CoreTrace/coretrace/issues/82), [#87](https://github.com/CoreTrace/coretrace/issues/87) | 2026-09-21 |
| [#56](https://github.com/CoreTrace/coretrace/issues/56) | minor | `CppCheckToolImplementation::name()` returns `"ikos"` | [#81](https://github.com/CoreTrace/coretrace/issues/81) | 2026-09-20 |
| [#57](https://github.com/CoreTrace/coretrace/issues/57) | minor | ikos receives `--format=text` with `--sarif-format` | [#81](https://github.com/CoreTrace/coretrace/issues/81) | 2026-09-20 |
| [#58](https://github.com/CoreTrace/coretrace/issues/58) | minor | tscancode SARIF written to a misspelled file name | [#81](https://github.com/CoreTrace/coretrace/issues/81) | 2026-09-20 |
| [#59](https://github.com/CoreTrace/coretrace/issues/59) | minor | HTTP server: 909 lines in a single header | [#86](https://github.com/CoreTrace/coretrace/issues/86) | 2026-09-21 |
| [#60](https://github.com/CoreTrace/coretrace/issues/60) | minor | Global `using json` aliases in shared headers | [#86](https://github.com/CoreTrace/coretrace/issues/86) | 2026-09-21 |
| [#61](https://github.com/CoreTrace/coretrace/issues/61) | minor | Server: CORS `*`, no body limit, optional shutdown token | [#85](https://github.com/CoreTrace/coretrace/issues/85) | 2026-09-21 |
| [#62](https://github.com/CoreTrace/coretrace/issues/62) | minor | `fork()` in a multithreaded process | [#67](https://github.com/CoreTrace/coretrace/issues/67) | 2026-09-19 |
| [#63](https://github.com/CoreTrace/coretrace/issues/63) | minor | Six speculative JSON manifest shapes | [#80](https://github.com/CoreTrace/coretrace/issues/80) | 2026-09-19 |
| [#64](https://github.com/CoreTrace/coretrace/issues/64) | minor | Hand-written help text diverges from the README | [#73](https://github.com/CoreTrace/coretrace/issues/73) | 2026-09-19 |
| [#65](https://github.com/CoreTrace/coretrace/issues/65) | — | Behaviours to preserve during the refactor (tracking) | [#87](https://github.com/CoreTrace/coretrace/issues/87) | 2026-09-21 |
| [#89](https://github.com/CoreTrace/coretrace/issues/89) | minor | Stack analyzer models inactive without `--config` | [#94](https://github.com/CoreTrace/coretrace/issues/94) | 2026-09-22 |
| [#90](https://github.com/CoreTrace/coretrace/issues/90) | critical | Diagnostics summary ignores external tools | [#95](https://github.com/CoreTrace/coretrace/issues/95) | 2026-09-22 |
| [#91](https://github.com/CoreTrace/coretrace/issues/91) | critical | Exit code always 0: findings never gate CI | [#96](https://github.com/CoreTrace/coretrace/issues/96) | 2026-09-22 |
| [#92](https://github.com/CoreTrace/coretrace/issues/92) | major | cppcheck invoked without options | [#97](https://github.com/CoreTrace/coretrace/issues/97) | 2026-09-22 |
| [#93](https://github.com/CoreTrace/coretrace/issues/93) | major | No merged SARIF document | [#98](https://github.com/CoreTrace/coretrace/issues/98) | 2026-09-22 |
| [#133](https://github.com/CoreTrace/coretrace/issues/133) | minor | cppcheck `-j` read from the stack analyzer config | [#160](https://github.com/CoreTrace/coretrace/issues/160) | 2026-10-02 |
| [#136](https://github.com/CoreTrace/coretrace/issues/136) | refactor | Shared state unsafe for concurrent tool runs | [#155](https://github.com/CoreTrace/coretrace/issues/155) | 2026-10-01 |
| [#142](https://github.com/CoreTrace/coretrace/issues/142) | major | ikos: C++ entry points mangled as one name | [#151](https://github.com/CoreTrace/coretrace/issues/151) | 2026-10-01 |
| [#143](https://github.com/CoreTrace/coretrace/issues/143) | minor | `splitByComma`: undefined behaviour on a lone quote | [#149](https://github.com/CoreTrace/coretrace/issues/149) | 2026-10-01 |
| [#144](https://github.com/CoreTrace/coretrace/issues/144) | minor | `mangleFunction`: parameter types inside the namespace | [#150](https://github.com/CoreTrace/coretrace/issues/150) | 2026-10-01 |
| [#167](https://github.com/CoreTrace/coretrace/issues/167) | refactor | `isMangled` in the public header | [#168](https://github.com/CoreTrace/coretrace/issues/168) | 2026-10-08 |

</details>

## Open debt

| | Debt | Area | Severity | Effect | Action | Issue |
|---|---|---|---|---|---|---|
| D1 | The pinned stack analyzer (v0.22.0) does not finish on CoreTrace's own code | dependencies | major | `--static` can run for hours on a real project | Release the analyzer from `main`, update the pin | [#169](https://github.com/CoreTrace/coretrace/issues/169) |
| D2 | The linked analyzers take no timeout (`docs/configuration.md`); [#137](https://github.com/CoreTrace/coretrace/issues/137) covered external tools only | architecture | major | A stuck analyzer stalls `ctrace`, and a server request with it | Run them in a child process, or give them a cancellation point | [#172](https://github.com/CoreTrace/coretrace/issues/172) |
| D3 | The in-process Clang overflows the stack of worker threads on macOS | dependencies | major | The concurrency analyzer crashes with `SIGBUS` under the default `--jobs` | Fix in `compilerlib` | [coretrace-compiler#204](https://github.com/CoreTrace/coretrace-compiler/issues/204) |
| D4 | The Windows backend does not compile; the port was reverted | platform | major | No Windows build | Resume the port behind CI | [#54](https://github.com/CoreTrace/coretrace/issues/54) |
| D5 | The analyzers replay `-Werror` from the compilation database | dependencies | minor | An input fails on a warning that only the analyzer's libc++ emits | Drop `-Werror` in both analyzers | [coretrace-stack-analyzer#214](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/214), [coretrace-concurrency-analyzer#199](https://github.com/CoreTrace/coretrace-concurrency-analyzer/issues/199) |
| D6 | Deprecated spellings kept for compatibility: `--async` and `runtime.async`, `--ipc=socket`, legacy config keys, root-level `invoke` and `input`. `--output-file` is accepted but no tool writes to it | code | minor | Code paths to maintain and test; one option that does nothing | Announce a removal release in `CHANGELOG.md`, then remove them | [#173](https://github.com/CoreTrace/coretrace/issues/173) |
| D7 | `cmake/flawfinder.cmake`, `tscancode.cmake` and `ikos.cmake` fetch `master`. `ikos.cmake` is included nowhere | build | minor | Builds with `ENABLE_FLAWFINDER` or `ENABLE_TSCANCODE` are not reproducible; one dead module | Pin a commit, or remove the options; delete `ikos.cmake` | [#174](https://github.com/CoreTrace/coretrace/issues/174) |
| D9 | coretrace-log declared under two names: `coretrace-logger` here, `coretrace_logger` in coretrace-compiler, which downloads it before checking whether the target exists | build | minor | Downloaded twice; two declarations of this dependency already broke the build once ([#37](https://github.com/CoreTrace/coretrace/issues/37), [#40](https://github.com/CoreTrace/coretrace/issues/40)) | Declare it as `coretrace_logger` | [#176](https://github.com/CoreTrace/coretrace/issues/176) |
| D10 | Unit tests cover 64.6% of the lines (89.6% with integration tests), with no threshold | tests | minor | Least covered: `ApiHandler.hpp` 64.7%, `FlawfinderToolImplementation.cpp` 72.6%, `config.hpp` 73.1%, `RunDirectory.hpp` 76.5%, `HttpServer.cpp` 79.3%, `StackAnalyzerToolImplementation.cpp` 79.4% | Tests for these files; a threshold once the test policy defines the critical path | [#164](https://github.com/CoreTrace/coretrace/issues/164) (`RunDirectory`) |
| D11 | Static analysis: `findStackAnalyzerSection` can take its `LoadContext` by `const&` (`ToolConfig.cpp:1074`); 16 raw loops flagged | code | minor | — | Fix the first; decide on the loops | [#177](https://github.com/CoreTrace/coretrace/issues/177) |
| D12 | The wiki's "Developer Build and Configuration" page still builds with `make` | docs | minor | A newcomer following the wiki gets another build than the README's | Align the page with the README | [#178](https://github.com/CoreTrace/coretrace/issues/178) |
| D13 | The `refactor` label points to a `ROADMAP.md` that does not exist | docs | minor | — | Write the roadmap (due in December), or fix the label | — |

## Debt in the analyzers

The analyzers track their own debt: coretrace-stack-analyzer has 33 open issues (27 bugs, mostly
false positives), coretrace-concurrency-analyzer 35 (32 bugs), coretrace-compiler 4. It affects
the findings `ctrace` reports, not its code. `ctrace` gets their fixes by updating its pins (D1).

## Next review

At the end of the project, together with the final static analysis: close or reassess each open
item, and add what the new analysis finds.
