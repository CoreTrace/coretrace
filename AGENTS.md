# AGENTS.md

Instructions for coding agents working in this repository. Humans: start with
[CONTRIBUTING.md](CONTRIBUTING.md). Update this file in the same pull request as the build,
test or architecture rule it describes.

## Scope

This repository is `ctrace`, the open-source CLI and HTTP server that runs analyzers on C, C++
and Python sources, merges their findings into one report (text, JSON, SARIF) and returns an
exit code that gates a CI pipeline. It is part of CoreTrace's open core, under Apache-2.0.

- The analyzers live in their own repositories and are fetched at pinned versions
  (`cmake/*.cmake`). A wrong or missing finding is fixed there, not here; `ctrace` only runs
  them, reads their reports and gates on them.
- The hosted CoreTrace platform is developed separately and is out of scope. Never move a
  feature out of this repository, and never add code that only the hosted service needs. The
  boundary is in [GOVERNANCE.md](GOVERNANCE.md) ("Commitments") and
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
./scripts/release/check-pinned-dependencies.sh
```

The first build fetches and compiles the analyzers and takes minutes on every core: ask the
maintainer before starting it, and prefer `-L unit` or one test (`ctest -R <name>`) while
iterating.

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

A change that breaks one of these is wrong, or is a significant change (see Boundaries).

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

## Development rules

- **Work from an issue.** A change no issue describes gets one first, to agree on the approach.
- **A change of behavior comes with its test, in the same pull request.** A bug fix comes with
  a test that fails without the fix. Write that test first.
- **Fix the cause where every caller passes**, not the symptom on the path the report names.
- **Unit tests** go in `tests/<component>_tests.cpp` as `ctrace_<component>_tests`, follow the
  `TestReport`/`expect` pattern of `tests/tool_output_tests.cpp`, and are added to
  `coretrace_unit_tests` in `CMakeLists.txt`. No network, no external tool, no dependence on
  timing or ordering.
- **Integration tests** are named after the behavior they check
  (`ctrace_exit_3_when_a_tool_cannot_run`).
- **An intermittent test is a bug to fix**, not to retry. Retries are for network steps only.
- **Documentation follows the change** in the same pull request: README, `docs/`, help text,
  and `CHANGELOG.md` when users see it.
- **Small pull requests, one subject each.** English Conventional Commit titles, such as
  `fix(cli): handle missing input`. Fill the pull request template: what and why, how it was
  tested.
- **Validate at the boundary**: CLI arguments, configuration files, HTTP request bodies, tool
  output. See [docs/quality/input-validation.md](docs/quality/input-validation.md).
- **Do not add what the task does not need**: no speculative option, abstraction or
  configuration shape. Several past debts were exactly that (#48, #63).
- Formatting is `clang-format`'s job and warnings are the compiler's: run the commands instead
  of reasoning about style. Every new source file starts with the SPDX header.

## Boundaries

- Never commit to `main`, force-push, or merge: `main` takes pull requests whose required
  checks pass. Commit, push or open an issue or a pull request only when the maintainer asks.
- Follow the [AI-assisted code policy](docs/quality/ai-assisted-code-policy.md). Commits carry
  no mark of the tool: no trailer, no co-author, no tool name. Declaring assistance is the
  author's choice, with the box of the pull request template.
- Never weaken a gate to make a change pass: `-Werror`, a test, a coverage floor, the pinned
  dependency check, the license check. If a gate is wrong, say so and stop.
- These are frozen interfaces. Changing one needs an issue open for 7 days
  ([GOVERNANCE.md](GOVERNANCE.md)): a command-line option, a configuration key, an exit code,
  the `coretrace/v1` SARIF fingerprint, the `coretrace-1.0` HTTP protocol. So do a new runtime
  dependency and a change of supported platform.
- A suspected vulnerability goes through [SECURITY.md](SECURITY.md), never a public issue.
- When stuck (the build does not configure, a test fails before your change, an invariant
  seems to block the task), report what you found instead of working around it.

## Technical debt

Debt is what makes each change slower or riskier than it should be. The issues are the source
of truth; [docs/quality/technical-debt.md](docs/quality/technical-debt.md) is the register that
summarizes them at each review.

### The rule for every change

Leave no new debt: a pull request must not break an invariant, lower coverage of the files it
touches, add an unpinned dependency, or leave a `TODO` without an issue number. Repay only the
debt that lies in the lines the task already changes. Debt seen elsewhere is recorded, not
fixed in passing: a drive-by fix hides in an unrelated diff and escapes review.

### The cycle

Run steps 1 to 3 whenever debt is noticed during a task. Run the whole cycle as a debt review
when the maintainer asks for one, and before each release.

1. **Observe.** Collect facts, not opinions: a broken invariant above, a duplicated source of
   truth, dead code or a dead option, a `TODO`, an unpinned or duplicated dependency, a file of
   the critical path under its coverage floor, a static analysis finding, documentation that
   contradicts the code, a test that depends on timing.
2. **Qualify against the architecture.** Name the invariant or the pipeline stage it harms,
   and what it costs: a wrong verdict, every change in an area, or one leftover. That gives the
   severity: `severity/critical` (blocks reliable use), `severity/major` (structural, costs
   every change), `severity/minor` (bug or leftover). Add `refactor` when the fix changes the
   architecture. If it harms nothing named here, it is a preference, not debt: drop it.
3. **Record.** One debt, one issue: the problem with its evidence (file, line, command and
   output), the fix, and acceptance criteria that a test or a command can check. Draft the
   issue and give it to the maintainer; create it only when asked. Check first that no open
   issue already covers it.
4. **Repay.** One pull request per issue, separate from features. Pin the current behavior
   with a test first, then change the structure, then show the test still passes. A repayment
   changes no behavior; if it must, that is a separate, announced change.
5. **Verify.** The pull request closes the issue only when its acceptance criteria hold and
   the gates pass: build with `-Werror`, `ctest`, format, licenses, pinned dependencies, and
   the coverage of the files touched not lower than before.
6. **Update the register.** Move the item from "Open debt" to "Repaid" in
   `docs/quality/technical-debt.md`, with the issue and the pull request. At a review, also
   re-read every open item: still true, still the same severity, or obsolete.

One fifth of each iteration is reserved for repayment. Order: critical first, then major debt
in the area the next roadmap item touches, then minor debt in batches. Debt that nothing
planned will ever touch may stay open, with its reason written in the issue.

### Where the result shows

```bash
gh issue list --label severity/critical --state open
gh issue list --label severity/major --state open
gh issue list --label severity/minor --state open
gh issue list --label refactor --state all --limit 100
```

- The register, [docs/quality/technical-debt.md](docs/quality/technical-debt.md): repaid and
  open debt, by review.
- Coverage: the `coverage-report` artifact of the "Coverage (llvm-cov)" job of each pull
  request, against the floors of the [test policy](docs/quality/test-policy.md); dated copies
  are kept in `docs/quality/coverage/`.
- Static analysis: `ctrace` on its own code, with the protocol of
  [docs/quality/static-analysis-initial.md](docs/quality/static-analysis-initial.md), so that
  two reports can be compared. cppcheck and clang-tidy alone, without building `ctrace`:
  [docs/quality/debt-baseline-2026-11-20.md](docs/quality/debt-baseline-2026-11-20.md).
- The four indicators of the
  [AI-assisted code policy](docs/quality/ai-assisted-code-policy.md): findings, debt added and
  repaid, time to fix a vulnerability, share of assisted pull requests.
- What the gates caught: [docs/quality/regressions.md](docs/quality/regressions.md).
