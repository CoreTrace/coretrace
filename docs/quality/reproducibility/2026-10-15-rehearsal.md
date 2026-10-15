# Reproducibility test of 2026-10-14: rehearsal

A rehearsal of the [reproducibility protocol](../reproducibility-protocol.md), run to find the
blockers before the test with an outside tester.

The test was conducted over SSH by tamiao19, a friend unfamiliar with the project.
No software had been installed specifically for the project. The documentation was
followed step by step. Machine times are measured;
human completion times are estimates that also include reading, typing, and troubleshooting.

| | |
|---|---|
| Date | 2026-10-14, from 10:52 to 11:14 |
| Run by |tamiao19, Friend, over SSH |
| Note | The `sudo` commands were typed separately, the SSH session having no administrator rights |
| Machine | Windows, WSL2, Ubuntu 22.04.2 LTS, x86-64, 12 cores, 9 GB of memory; GCC 11.4, CMake 3.22.1, no LLVM, no Ninja, no cppcheck |
| Commit tested | `962f8dc` |

The README tests the build on Ubuntu 24.04; Ubuntu 22.04 is what many WSL installations still
run, and the release archives support it.

## Results

| # | Task | Estimated human completion time (Ubuntu 22.04) | Machine time | Done | Notes |
|---|---|---|---|---|---|
| T1 | Understand what `ctrace` does | 2–3 min | — | yes | About 1 minute to read the 85 words before the quick start, plus time to understand the purpose and expected output |
| T2 | Quick start with the release archive | 2–4 min | 5 s | yes | Read and run the commands, then check the overflow finding, exit code 2, and generated SARIF report |
| T3 | Build from source | 45–75 min | 6 min | yes, after B1 to B3 | Most time goes into investigating CMake and compiler requirements (B1 and B2), installing dependencies, and handling B3; machine work includes LLVM 20: 50 s, GCC 13: 16 s, configure: 12 s, build: 4 min 05 s |
| T4 | Test suite | 10–15 min | 30 s | yes, after B4 | Inspect the 17 failures, identify the missing cppcheck dependency (B4), install it, and rerun: 77 of 77 tests pass in 15 s |
| T5 | Analyze a file with the local build, SARIF report | 3–5 min | < 1 s | yes | Run the analysis and inspect the SARIF finding; allow time to compare it with the source (a false positive; see "Other observations") |
| T6 | Find a first contribution | 5–10 min | — | yes | Read the contribution guidance and welcome tickets, check their availability, and choose a suitable issue |
| | **Total** | **67–112 min (about 1 h 10 min to 2 h)** | | **all done** | Sum of the task estimates, including execution and waiting time; most uncertainty comes from troubleshooting B1 and B2 |

**These are planning estimates, not measured human durations.** They describe a newcomer
following the documentation as it stood before the corrections, on Ubuntu 22.04, with the same
blockers. The observed rehearsal lasted 22 minutes, including about 11 minutes waiting for
the `sudo` commands; reading, searching, and typing were previously estimated to add 45 to 90
minutes. The per-task estimates distribute that overall allowance and should be validated
with an outside tester.

On Ubuntu 24.04, where B1 and B2 do not occur, the README's commands alone should take about
15 minutes, mostly downloads and the build. Reading, inspecting the results, and choosing a
first contribution require additional time.

## Blockers

| # | Task | Observed at | What happened | Observed delay | Estimated human resolution time | How it ended | Cause | Correction |
|---|---|---|---|---|---|---|---|---|
| B1 | T3 | 10:52:31 | `CMake 3.28 or higher is required. You are running version 3.22.1`. The README requires 3.28 but gives no way to get it; `apt-get install cmake` gives 3.22. Installing it with `pip` failed too: neither `pip` nor `python3-venv` is installed, and both need `sudo` | 1 min (machine time) | 10–20 min to investigate installation options, find a compatible CMake binary, install it, and retry configuration | Alone: CMake 3.31.6 binary from Kitware's releases | Documentation | The README gives the Ubuntu 22.04 commands |
| B2 | T3 | 11:02:20 | `logger.hpp:6:10: fatal error: format: No such file or directory`. GCC 11 has no `<format>`, and Clang 20 uses GCC 11's standard library, so it fails the same way. The README asks only for "a C++20 compiler"; the release `Dockerfile` already says "C++20 `<format>` needs GCC 13" | 5 min | 20–35 min to identify the standard-library requirement, find and install GCC 13, and retry with the correct compiler | With help: GCC 13 from the `ubuntu-toolchain-r/test` PPA, as the release build does | Documentation | The README names GCC 13 and gives the PPA |
| B3 | T3 | 10:53:18 | LLVM 20 not found: its install needs `sudo`, which the session did not have | 7 min waiting | About 7 min waiting, assuming the same administrator availability as in the rehearsal | tamiao19 ran the README's command | The rehearsal's setup, not the documentation | — |
| B4 | T4 | 11:12:28 | 17 of 77 integration tests fail: `Command not found in PATH: cppcheck`. The README never mentions cppcheck; CI installs it | 2 min | 5–10 min to inspect the failures, identify and install cppcheck, and confirm that the tests pass | `sudo apt-get install cppcheck` (2.7 on Ubuntu 22.04): 77 of 77 pass | Documentation | cppcheck added to the README's commands |

**The human resolution times are planning estimates for the original documentation on Ubuntu
22.04.** They include investigation, commands, and the associated execution or waiting time.
They are already included in T3 and T4 above and must not be added to the Results total.
B3 uses the observed wait as a scenario assumption, not a general estimate: it depends on
administrator availability and does not apply when the tester has their own `sudo` access.

The README's LLVM script worked as is on Ubuntu 22.04: it reads the distribution's codename, and
apt.llvm.org has packages for it.

## Other observations

- **T5 found a false positive.** A buffer written by `strcpy` inside a helper is reported as
  "never initialized". It is the family of
  [coretrace-stack-analyzer#224](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/224)
  and [#203](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/203).
- **T6:** of the three welcome tickets, #164 is already assigned; #175 says which files and lines
  to change and how to check.
- **The README's BUILD section does not say to clone the repository.** It did not stop this
  rehearsal; To check during the next independent test.
- **The GCC 13 PPA also upgrades the system's `libstdc++6` and `libgcc-s1`**, to the versions of
  GCC 16. The README now says so.
- **Side effects on the machine:** the apt.llvm.org and `ubuntu-toolchain-r/test` repositories,
  and the packages `llvm-20`, `clang-20`, `g++-13`, `ninja-build` and `cppcheck`. The test
  directory is `~/coretrace-repro-2026-10-15`.

## Corrections

- The README now names the compiler (GCC 13 or newer, or a standard library with `<format>`),
  adds cppcheck to the Ubuntu and macOS commands, and gives the commands for Ubuntu 22.04.
  These commands were then run as written in a clean Ubuntu 22.04 container (arm64, so the CMake
  archive is chosen by architecture): installs and configure in 63 s, build in 2 min 37 s, 77 of 77
  tests passed.

## Conclusion

On Ubuntu 22.04, the documentation before this rehearsal did not lead to a build: it hid three
prerequisites (CMake 3.28 from outside the distribution, GCC 13, cppcheck) that the team's own
release build already handles. The release archive, on the other hand, worked in 5 seconds.

