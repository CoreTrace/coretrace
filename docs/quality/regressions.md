# Regressions caught

Changes that broke something that worked, caught by a test or by CI, with their dates, the
tests that caught them and how they were fixed. Also listed: the false alarms, the other
failures, and the regression tests added with each bug fix.

| | |
|---|---|
| Date | 2026-10-13 |
| Sources | The 42 failed CI runs of this repository, all workflows, from 2025-10-30 to 2026-10-13; their logs (GitHub keeps them 90 days, so the details start in July 2026); the pull requests and commits involved |

## The 42 failed runs

| Category | Runs | What they were |
|---|---:|---|
| Regressions caught | 7 | 5 changes that broke existing behavior (below) |
| False alarm | 1 | A test that was too strict (below) |
| Network | 9 | Downloads of apt packages, `llvm.sh` or Docker images that failed |
| Formatting | 11 | `clang-format` refusing unformatted code |
| Work in progress | 11 | New features or CI jobs that did not build yet |
| `main` in 2025-11 | 3 | Direct pushes, before pull requests were the rule; logs expired |

## Regressions caught

| | Date | Change | Caught by | What broke | Fix | When |
|---|---|---|---|---|---|---|
| R1 | 2026-09-20 | [#82](https://github.com/CoreTrace/coretrace/pull/82), compiler warnings as errors | "Build Linux binary", the release toolchain (GCC 13) | `tests/config_parser_tests.cpp`: unused variable `ok`, four times. The checks were `assert()` calls, which a release build removes: `ctrace_config_tests` checked nothing in that build | `c47bec2`, "make the config checks survive NDEBUG", 7 minutes later | Before merge |
| R2 | 2026-09-22 | [#96](https://github.com/CoreTrace/coretrace/pull/96) and [#98](https://github.com/CoreTrace/coretrace/pull/98), merged at 13:59 and 14:00; each was green on its own base | The F7 async harness, Ubuntu and macOS | "sync cppcheck run failed with exit code 2": with the new gate, cppcheck's error findings make `ctrace` exit 2, which the harness took for a failed run | [#99](https://github.com/CoreTrace/coretrace/pull/99): the harness runs with `--fail-on none`, 18 minutes later | After merge, on `main` |
| R3 | 2026-09-09 | [#36](https://github.com/CoreTrace/coretrace/pull/36), Windows port; its CI passed in April, and it was merged five months later | "Build on Windows" and "Build Windows binary" | `'cxxabi.h' file not found` in the stack analyzer's `mangle.hpp`, and an unexpected `clang-cl` version. On Linux, the build also broke for a contributor ([#40](https://github.com/CoreTrace/coretrace/issues/40): the logger target defined twice), which CI did not catch | Revert `4962527`, 2026-09-13 | After merge; partly caught |
| R4 | 2026-09-28 | [#121](https://github.com/CoreTrace/coretrace/pull/121), concurrency analyzer linked into `ctrace` | "Build Linux binary": `tests/release/check-concurrency-analysis.sh` on the distribution image | The packaged analyzer could not compile `race.c` ("The following arguments were not expected") | `0e7574a`, "compile the concurrency analyzer's units in memory", the same day | Before merge |
| R5 | 2026-10-02 | [#162](https://github.com/CoreTrace/coretrace/pull/162), Dependabot: `pyyaml` 5.3 → 5.4 in `tests/python/project` | `ctrace_python_analyzer_standalone_runs_anywhere` and `ctrace_install_ships_python_analyzer`, Ubuntu and macOS; release verification | "the project's vulnerable pin in project/requirements.txt:2:1 is not reported": the test project pins a vulnerable version on purpose | Pull request closed, not merged | Before merge |

Three of the five were caught before merge. R2 and R3 reached `main`, where CI caught them on
the merge itself: R2 was fixed 18 minutes later, R3 reverted four days later.

## False alarms

| | Date | Test | Cause | Fix |
|---|---|---|---|---|
| F1 | 2026-09-21 | `ctrace_version_is_resolved`, in [#87](https://github.com/CoreTrace/coretrace/pull/87) | The expected version did not allow a bare commit hash, which a CI clone without tags produces | `744104c`: the test accepts the hash |
| F2 | 2026-10-13, again 2026-10-15 | `ctrace_tool_scheduling_tests`, on `main` after [#181](https://github.com/CoreTrace/coretrace/pull/181), then after [#193](https://github.com/CoreTrace/coretrace/pull/193), macOS only | The test compares wall-clock end times with a 100–150 ms margin, which a slow runner can eat | Job re-run, green. To do: rewrite the test without timing ([test policy](test-policy.md), convention 4) |

F2 is not among the 42 runs: both times, the re-run passed, so the run is recorded as a success.
Two failures in three days make it the first fix of the test suite to do.

## Other failures

- **Network, 9 runs** (2026-07-15 to 2026-09-24): failed downloads of apt packages, `llvm.sh`
  or Docker images. The log of the 2026-07-15 pull request run has expired; it is classified
  here by its job, its step and its date, the same as the `main` run of that day. Retries were
  added in [#111](https://github.com/CoreTrace/coretrace/pull/111) (2026-09-25) and
  [#119](https://github.com/CoreTrace/coretrace/pull/119) (2026-09-26). No network failure
  since.
- **Formatting, 11 runs**, the last on 2026-10-04, in the external contribution
  [#166](https://github.com/CoreTrace/coretrace/pull/166), fixed by its author before merge.
- **Work in progress, 11 runs**: [#6](https://github.com/CoreTrace/coretrace/pull/6),
  [#19](https://github.com/CoreTrace/coretrace/pull/19),
  [#20](https://github.com/CoreTrace/coretrace/pull/20), the Windows jobs of
  [#36](https://github.com/CoreTrace/coretrace/pull/36) in April,
  [#70](https://github.com/CoreTrace/coretrace/pull/70) (static LLVM, abandoned), and the new
  coverage job of [#116](https://github.com/CoreTrace/coretrace/pull/116).

## Regression tests added with bug fixes

Each of these fixes added a test that fails if the bug comes back.

| Bug | Fixed by | Date | Tests |
|---|---|---|---|
| [#44](https://github.com/CoreTrace/coretrace/issues/44) External tool exit status never checked | [#67](https://github.com/CoreTrace/coretrace/pull/67) | 2026-09-19 | `ctrace_process_tests` |
| [#43](https://github.com/CoreTrace/coretrace/issues/43) Concurrent server requests race | [#69](https://github.com/CoreTrace/coretrace/pull/69) | 2026-09-19 | `ctrace_server_concurrency_tests` |
| [#89](https://github.com/CoreTrace/coretrace/issues/89) Models inactive without `--config` | [#94](https://github.com/CoreTrace/coretrace/pull/94) | 2026-09-22 | `ctrace_default_models_without_config` |
| [#90](https://github.com/CoreTrace/coretrace/issues/90) Summary ignores external tools | [#95](https://github.com/CoreTrace/coretrace/pull/95) | 2026-09-22 | `ctrace_tool_diagnostics_tests`, `ctrace_cppcheck_diagnostics_are_counted`, `ctrace_uninterpreted_tool_is_reported` |
| [#91](https://github.com/CoreTrace/coretrace/issues/91) Exit code always 0 | [#96](https://github.com/CoreTrace/coretrace/pull/96) | 2026-09-22 | `ctrace_exit_policy_tests`, `ctrace_exit_2_on_error_finding`, `ctrace_exit_0_when_below_threshold`, `ctrace_exit_0_with_fail_on_none`, `ctrace_exit_3_when_a_tool_cannot_run` |
| [#135](https://github.com/CoreTrace/coretrace/issues/135) Output order depends on timing | [#154](https://github.com/CoreTrace/coretrace/pull/154) | 2026-10-01 | `ctrace_tool_scheduling_tests` |
| [#142](https://github.com/CoreTrace/coretrace/issues/142) ikos entry points mangled as one name | [#151](https://github.com/CoreTrace/coretrace/pull/151) | 2026-10-01 | `ctrace_tool_arguments_tests` |
| [#143](https://github.com/CoreTrace/coretrace/issues/143) `splitByComma` undefined behaviour | [#149](https://github.com/CoreTrace/coretrace/pull/149) | 2026-10-01 | `ctrace_strings_tests` |
| [#144](https://github.com/CoreTrace/coretrace/issues/144) Parameter types inside the namespace | [#150](https://github.com/CoreTrace/coretrace/pull/150) | 2026-10-01 | `ctrace_mangle_tests` |
| [#133](https://github.com/CoreTrace/coretrace/issues/133) cppcheck `-j` read from the wrong config | [#160](https://github.com/CoreTrace/coretrace/pull/160) | 2026-10-02 | `ctrace_config_tests`, `ctrace_tool_arguments_tests` |
| [#146](https://github.com/CoreTrace/coretrace/issues/146) `isMangled` reads past a string slice | [#166](https://github.com/CoreTrace/coretrace/pull/166) | 2026-10-04 | `ctrace_mangle_tests`; the slice test fails on the code before the fix |

## Lessons and actions

1. **R2 and R3 passed CI against an old `main`, then broke it once merged.** #98 was tested
   before #96 landed, and #36 five months before its merge. Requiring a pull request to be up to
   date with `main` before merging (the ruleset's "require branches to be up to date", off today)
   would have caught both before merge. The cost: updating the branch before each merge.
2. **`assert()` is not a test (R1).** A release build removes it. Unit tests use `TestReport`
   and `expect`, which are always compiled ([test policy](test-policy.md), convention 2).
3. **The vulnerable test project must say so (R5).** Add a comment to
   `tests/python/project/requirements.txt` explaining that the pin is deliberate, and dismiss
   the Dependabot alert as test code, so that it stops proposing the upgrade.
4. **Rewrite `ctrace_tool_scheduling_tests` without timing (F2).**
