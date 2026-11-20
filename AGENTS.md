# AGENTS.md

Instructions for coding agents. Humans: start with [CONTRIBUTING.md](CONTRIBUTING.md).

This file has two parts. **Part 1** describes the CoreTrace project as a whole and the rules
shared by its repositories; it is meant to be the same in each of them. **Part 2** describes
this repository. Where they disagree, Part 2 wins for this repository. Update this file in the
same pull request as the rule, command or architecture it describes.

# Part 1: the CoreTrace project

## What CoreTrace is

CoreTrace finds bugs in C, C++ and Python code by running several analyzers and merging what
they find into one report and one verdict that can gate a CI pipeline. It is split into
repositories of the [CoreTrace organization](https://github.com/CoreTrace), each versioned and
released on its own.

The open repositories are the open core of a hosted analysis service, the CoreTrace platform,
which is developed separately and is out of scope here: never move a feature out of an open
repository, and never add code that only the hosted service needs.

## How the repositories fit together

```
  clients                         the product
  ┌──────────────────────────┐    ┌──────────────────────────────────────────────┐
  │ coretrace-gui (desktop)  │    │ coretrace: ctrace                            │
  │ coretrace-vscode (editor)├───▶│ runs the tools, merges their findings,       │
  │ a CI pipeline            │    │ gives the verdict                            │
  └──────────────────────────┘    └───────┬──────────────────────────┬───────────┘
                                          │ links, in process        │ runs, as processes
                              ┌───────────┴──────────┐    ┌──────────┴──────────────┐
                              │ stack analyzer       │    │ runtime analyzer        │
                              │ concurrency analyzer │    │ python analyzer         │
                              └───────────┬──────────┘    │ cppcheck, flawfinder... │
                                          │ compile       └─────────────────────────┘
                                          ▼ through
                              ┌──────────────────────┐
                              │ coretrace-compiler   │
                              └──────────────────────┘
```

| Repository | Role | How it joins the product |
|---|---|---|
| `coretrace` | `ctrace`, the CLI and HTTP server: runs the tools, normalizes and merges their findings, returns the exit code | The entry point; the release archive is built here |
| `coretrace-stack-analyzer` | Stack, buffer, pointer-escape and initialization analysis on LLVM IR | Linked into `ctrace`, at a pinned tag |
| `coretrace-concurrency-analyzer` | Data races, missing joins, lock-order deadlocks | Linked into `ctrace`, at a pinned tag |
| `coretrace-runtime-analyzer` | Instrumented runs: bounds, allocations, vtables | Shipped next to `ctrace`, run as a separate process |
| `coretrace-python-analyzer` | Analysis of Python projects | Shipped next to `ctrace` as a standalone executable, run as a separate process |
| `coretrace-compiler` | Clang integration library | The analyzers compile their inputs through it; one `ctrace` build holds one version of it |
| `coretrace-log` | C++20 logging library | Fetched by `coretrace`, the stack analyzer and `coretrace-compiler` |
| `coretrace-gui` | Desktop interface (Tauri) | A client: it runs an installed `ctrace` |
| `coretrace-vscode` | VS Code extension | A client: it runs `ctrace` and shows its findings in the editor |
| `coretrace-tool-template` | Scaffold for a new tool | Where a convention shared by the tools starts |
| `coretrace-ci-consumer-demo` | Reference CI integration | Uses a release as an outside project would |

`ctrace` also runs third-party tools when they are installed: cppcheck, flawfinder, ikos, and
TscanCode (a fork kept in `coretrace-TscanCode`).

### What holds them together

These are the contracts between the repositories. Whoever changes one breaks the others, so
each change is proposed in an issue first and stays open for comments for 7 days, as
[GOVERNANCE.md](https://github.com/CoreTrace/coretrace/blob/main/GOVERNANCE.md) sets:

- **The finding model.** Every tool's output is normalized by `ctrace` to one model, written as
  text, JSON or SARIF. In SARIF, a finding is identified by its `coretrace/v1` fingerprint.
- **The verdict.** Exit code `0` clean, `1` usage error, `2` findings, `3` incomplete analysis.
  A tool that could not run, or could not be read, is never counted as clean.
- **The interfaces of `ctrace`**: its command-line options, its configuration schema, and the
  `coretrace-1.0` HTTP protocol of its server.
- **Pinned versions.** A repository takes another at a tag or a commit, never a branch. A fix
  in an analyzer reaches users in two steps: a release of the analyzer, then an update of the
  pin in `coretrace`.
- **One compiler library per build.** The analyzers linked into `ctrace` must pin the same
  `coretrace-compiler` commit; moving it means releasing them together.

### Where a problem is fixed

| Symptom | Repository |
|---|---|
| A wrong, missing or duplicated finding | The analyzer that produced it |
| An input that does not compile for the analyzers | `coretrace-compiler` |
| A finding lost or miscounted in the merged report, a wrong exit code, a tool not run | `coretrace` |
| A report shown wrongly | The client: `coretrace-gui` or `coretrace-vscode` |

Do not work around another repository's defect here: record it there, and take the fix by
updating the pin.

## How the project is organized

- **Decisions are made in the open**, in issues and pull requests, by the rules of
  [GOVERNANCE.md](https://github.com/CoreTrace/coretrace/blob/main/GOVERNANCE.md). The default
  branch takes pull requests whose required checks pass.
- **Not every repository is maintained the same way.** The product and its desktop interface
  welcome contributors; others are maintained without contribution calls, or published as is.
  The levels are in the
  [open-source scope](https://github.com/CoreTrace/coretrace/blob/main/docs/open-source-scope.md).
- **Licenses differ.** Most repositories are under Apache-2.0, some under MIT or GPL-3.0. Use
  the license and the file header of the repository you are in, and never copy code from one
  repository into another without checking that its license allows it.
- **Each repository is versioned on its own**, with semantic versioning; components in `0.x`
  may still change incompatibly between minor versions.

## Rules for every repository

### Development

- **Work from an issue.** A change no issue describes gets one first, to agree on the approach.
- **A change of behavior comes with its test, in the same pull request.** A bug fix comes with
  a test that fails without the fix. Write that test first.
- **Fix the cause where every caller passes**, not the symptom on the path the report names.
- **An intermittent test is a bug to fix**, not to retry. Retries are for network steps only.
- **Documentation follows the change** in the same pull request: README, `docs/`, help text,
  and the changelog when users see it.
- **Small pull requests, one subject each.** English Conventional Commit titles, such as
  `fix(cli): handle missing input`. Fill the pull request template where there is one: what
  and why, how it was tested.
- **Validate at the boundary**: arguments, configuration files, request bodies, the output of
  another tool.
- **Do not add what the task does not need**: no speculative option, abstraction or
  configuration shape.
- **Use the repository's scripts and tools** for formatting, license headers and other checks:
  run them and act on what they report, rather than checking by reading.

### Boundaries

- Never commit to the default branch, force-push, or merge. Commit, push or open an issue or
  a pull request only when the maintainer asks.
- A commit uses the git identity already configured, and holds a subject and a body only: no
  co-author and no other trailer.
- Never weaken a gate to make a change pass: a compiler warning, a test, a coverage floor, a
  dependency or license check. If a gate is wrong, say so and stop.
- Ask before a run that keeps most cores busy for more than about a minute: a full build, the
  whole test suite, a benchmark.
- A suspected vulnerability goes through the repository's `SECURITY.md`, never a public issue.
- When stuck (the build does not configure, a test fails before your change, a rule seems to
  block the task), report what you found instead of working around it.

### Technical debt

Debt is what makes each change slower or riskier than it should be. Each debt is an issue of
the repository it lies in; the issues are the source of truth.

**The rule for every change.** Leave no new debt: a pull request must not break an invariant
of its repository, lower the coverage of the files it touches, add an unpinned dependency, or
leave a `TODO` without an issue number. Repay only the debt that lies in the lines the task
already changes. Debt seen elsewhere is recorded, not fixed in passing: a drive-by fix hides
in an unrelated diff and escapes review.

**The cycle.** Run steps 1 to 3 whenever debt is noticed during a task. Run the whole cycle as
a debt review when the maintainer asks for one, and before each release.

1. **Observe.** Collect facts, not opinions: a broken invariant, a duplicated source of truth,
   dead code or a dead option, a `TODO`, an unpinned or duplicated dependency, a file under its
   coverage floor, a static analysis finding, documentation that contradicts the code, a test
   that depends on timing.
2. **Qualify against the architecture.** Name the invariant or the contract it harms, in this
   repository or between repositories, and what it costs: a wrong result for users, every
   change in an area, or one leftover. That gives the severity: `severity/critical` (blocks
   reliable use), `severity/major` (structural, costs every change), `severity/minor` (bug or
   leftover). Add `refactor` when the fix changes the architecture. If it harms nothing named,
   it is a preference, not debt: drop it.
3. **Record.** One debt, one issue, in the repository where the fix belongs: the problem with
   its evidence (file, line, command and output), the fix, and acceptance criteria that a test
   or a command can check. Draft the issue and give it to the maintainer; create it only when
   asked. Check first that no open issue already covers it.
4. **Repay.** One pull request per issue, separate from features. Pin the current behavior
   with a test first, then change the structure, then show the test still passes. A repayment
   changes no behavior; if it must, that is a separate, announced change.
5. **Verify.** The pull request closes the issue only when its acceptance criteria hold, the
   gates pass, and the coverage of the files touched is not lower than before.
6. **Update the register**, where the repository keeps one: move the item from open to repaid,
   with the issue and the pull request. At a review, also re-read every open item: still true,
   still the same severity, or obsolete.

One fifth of each iteration is reserved for repayment. Order: critical first, then major debt
in the area the next roadmap item touches, then minor debt in batches. Debt that nothing
planned will ever touch may stay open, with its reason written in the issue.

**Where the result shows**, in any repository:

```bash
gh issue list --label severity/critical --state open
gh issue list --label severity/major --state open
gh issue list --label severity/minor --state open
gh issue list --label refactor --state all --limit 100
```

Three indicators are measured the same way at each debt review: static analysis findings, debt
added and repaid in the period, and the time to fix a vulnerability.

# Part 2: this repository, `coretrace`

## Scope

This repository is `ctrace`, under Apache-2.0: the CLI and HTTP server that runs the analyzers
on C, C++ and Python sources, merges their findings into one report (text, JSON, SARIF) and
returns the exit code. It owns the contracts of Part 1: the finding model, the verdict and the
interfaces of `ctrace`.

The analyzers are fetched at pinned versions (`cmake/*.cmake`). `ctrace` only runs them, reads
their reports and gates on them; the boundary with the hosted platform is in
[GOVERNANCE.md](GOVERNANCE.md) ("Commitments") and
[docs/open-source-scope.md](docs/open-source-scope.md).

## Commands

Configure once, as the [README](README.md#build) says for your platform (LLVM/Clang 20, CMake
3.28+, Ninja). Then:

```bash
cmake --build build --parallel 4             # warnings are errors (CORETRACE_WERROR=ON)
ctest --test-dir build --output-on-failure   # everything; needs cppcheck installed
ctest --test-dir build -L unit               # in process, about 7 s
ctest --test-dir build -L integration        # the real binary on the fixtures of tests/
./scripts/format-check.sh                    # ./scripts/format.sh applies the format
./scripts/check-license-compliance.sh        # SPDX header on every source file
sh scripts/release/check-pinned-dependencies.sh .   # the argument is the source directory
```

The first build fetches and compiles the analyzers and takes minutes on every core: ask the
maintainer before starting it, and prefer `-L unit` or one test (`ctest -R <name>`) while
iterating.

### Scripts

| When | Script | What it does |
|---|---|---|
| After changing C++ | `./scripts/format.sh`, then `./scripts/format-check.sh` | Applies, then verifies, the `clang-format` style on the project's own sources. CI runs the check with `clang-format` 20 |
| After adding a file | `./scripts/check-license-compliance.sh` | Fails on a tracked source, CMake, shell, Python or YAML file without `SPDX-License-Identifier: Apache-2.0` in its first 12 lines, and names it. It checks; it does not add the header: write it as the first line (`//` in C++, `#` elsewhere) |
| After touching `cmake/*.cmake` | `sh scripts/release/check-pinned-dependencies.sh .` | Fails on a dependency fetched from a branch, or a download without its hash |
| To measure coverage | `sh scripts/ci/coverage.sh build coverage` | Needs a build configured with `-DCORETRACE_COVERAGE=ON` (Clang), so a full build: ask first |
| In a CI step that uses the network | `scripts/ci/retry.sh` | Retries a command with a growing delay. Never for a test |

No script checks commit messages in this repository: nothing will catch a wrong one. Write the
subject as an English Conventional Commit yourself, `type(scope): summary`, as the pull request
template asks for the title.

## Architecture

One pipeline, from the inputs to the verdict. It is the critical path of the
[test policy](docs/quality/test-policy.md): the worst failure of `ctrace` is a wrong verdict.

| Stage | Where | Role |
|---|---|---|
| 1. Configuration | `src/App/Config.cpp`, `ToolConfig.cpp`, `ShippedDefaults.cpp`, `include/Config/config.hpp` | Built-in defaults < `--config` file < CLI, into one `ProgramConfig` (one struct per section) |
| 2. Inputs | `src/App/Files.cpp`, `src/Process/Tools/CompileCommands.cpp`, `include/Process/Tools/InputPaths.hpp` | Resolve the files; each goes to the tools of its language |
| 3. Tool runs | `include/Process/Tools/ToolsInvoker.hpp`, `src/Process/Tools/*ToolImplementation.cpp`, `include/Process/UnixProcess.hpp` | `ToolInvoker` schedules `IAnalysisTool`s on a thread pool; each writes to a `ToolOutput` |
| 4. Findings | `include/Process/Tools/Diagnostic.hpp`, `src/Process/Tools/Sarif.cpp`, `ReportFile.hpp` | Normalize, order, count, merge |
| 5. Verdict | `include/App/ExitPolicy.hpp`, `main.cpp` | `0` clean, `1` usage, `2` findings, `3` incomplete |

Server mode (`--ipc serve`, `src/Process/Ipc/`) wraps the same pipeline per HTTP request and
returns the same verdict in `result.gate`. `src/App/Runner.cpp` is the entry of both modes.

Two kinds of tools:

- **Linked in process**: the stack and concurrency analyzers (`runsInProcess` in
  `include/App/SupportedTools.hpp`).
- **External processes**: cppcheck, flawfinder, tscancode, ikos, and the Python and runtime
  analyzers bundled under `libexec/coretrace/`.

### Invariants

A change that breaks one of these is wrong, or changes a contract of Part 1.

1. **Tools know only `IAnalysisTool` and `ToolOutput`.** A tool normalizes its own format to
   `Diagnostic`; counting, gating and merging never see a tool-specific format.
2. **Unknown is never clean.** A tool that could not run, or whose output could not be read,
   marks its `ToolOutput` failed or unread, and the run exits with 3. Zero findings is a fact
   only once the output was interpreted. An incomplete run wins over findings.
3. **Only `main.cpp` decides the exit code.** No `std::exit` or `abort` elsewhere: return the
   error.
4. **One configuration, one schema.** A new setting is added to its `ProgramConfig` section,
   the loader, the CLI and `docs/configuration.md` together.
5. **One output channel.** Tool results go through `ToolOutput`; status and progress go through
   the logger. A tool never writes to `std::cout` or `std::cerr` itself.
6. **Output is deterministic.** Diagnostics are reported in a fixed order, whatever order the
   tools finished in; sequential and parallel runs give the same fingerprints (F7 harness).
7. **In-process analyzers run under `inProcessAnalysisMutex()`**
   (`include/Process/Tools/InProcessAnalysis.hpp`), until they are proven thread-safe.
8. **Dependencies are pinned** to a tag or commit, never a branch.

### Adding or changing a tool

A new tool is a significant change: an accepted issue comes first. Then it touches
`SUPPORTED_TOOLS` (`include/App/SupportedTools.hpp`), `registerTool` and the static or dynamic
list in `ToolsInvoker.hpp`, a class in `include/Process/Tools/AnalysisTools.hpp` with its
`src/Process/Tools/<Name>ToolImplementation.cpp`, `CMakeLists.txt`, `docs/configuration.md`,
the README, and tests with a fake tool (`tests/fake-*.sh`) for its failure paths.

## Tests

- **Unit tests** go in `tests/<component>_tests.cpp` as `ctrace_<component>_tests`, follow the
  `TestReport`/`expect` pattern of `tests/tool_output_tests.cpp`, and are added to
  `coretrace_unit_tests` in `CMakeLists.txt`. No network, no external tool, no dependence on
  timing or ordering.
- **Integration tests** are named after the behavior they check
  (`ctrace_exit_3_when_a_tool_cannot_run`).
- What to validate at the boundary, and how:
  [docs/quality/input-validation.md](docs/quality/input-validation.md).
- Past debts came from options and shapes nobody needed (#48, #63): the rule of Part 1 against
  speculative additions is not theoretical here.

## Technical debt in this repository

- The register is [docs/quality/technical-debt.md](docs/quality/technical-debt.md): repaid and
  open debt, by review. Step 6 of the cycle updates it.
- The gates of step 5 are: build with `-Werror`, `ctest`, format, license headers, pinned
  dependencies.
- Coverage: the `coverage-report` artifact of the "Coverage (llvm-cov)" job of each pull
  request, against the floors of the [test policy](docs/quality/test-policy.md); dated copies
  are kept in `docs/quality/coverage/`.
- Static analysis: `ctrace` on its own code, with the protocol of
  [docs/quality/static-analysis-initial.md](docs/quality/static-analysis-initial.md), so that
  two reports can be compared. cppcheck and clang-tidy alone, without building `ctrace`:
  [docs/quality/debt-baseline-2026-11-20.md](docs/quality/debt-baseline-2026-11-20.md).
- What the gates caught: [docs/quality/regressions.md](docs/quality/regressions.md).
