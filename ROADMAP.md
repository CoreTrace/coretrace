# Roadmap

Where CoreTrace's open repositories are going, until the end of the EIP and after it. Each item
links to the issue that tracks it; items without an issue get one when work starts.

| | |
|---|---|
| Updated | 2026-10-14 |
| Horizon | Q4 2026 (October to December), Q1 2027 (January to March), then after the EIP |
| Decided by | The lead maintainer, under [GOVERNANCE.md](GOVERNANCE.md); changes to this file go through a pull request |

Four themes, in order of priority.

## 1. Stabilize `ctrace`

Goal: a `ctrace` that always finishes, gives the right verdict, and keeps its interfaces stable,
then version 1.0.

**Q4 2026**

- Update the stack analyzer past v0.22.0, which does not finish on CoreTrace's own code
  ([#169](https://github.com/CoreTrace/coretrace/issues/169)).
- Stop a stuck linked analyzer after its timeout, as external tools already are
  ([#172](https://github.com/CoreTrace/coretrace/issues/172)).
- Enforce the coverage floors of the [test policy](docs/quality/test-policy.md) in CI, and rewrite
  the timing-dependent `ctrace_tool_scheduling_tests`
  ([Regressions caught](docs/quality/regressions.md), F2).
- Repay the minor debt of the [register](docs/quality/technical-debt.md): pinned optional tools
  ([#174](https://github.com/CoreTrace/coretrace/issues/174)), CLI11
  ([#175](https://github.com/CoreTrace/coretrace/issues/175)), the logger declared twice
  ([#176](https://github.com/CoreTrace/coretrace/issues/176)).
- Final static analysis, with the protocol of the
  [initial report](docs/quality/static-analysis-initial.md).

**Q1 2027**

- Announce, then remove, the deprecated spellings
  ([#173](https://github.com/CoreTrace/coretrace/issues/173)).
- Freeze the public interfaces: command-line options, configuration schema, exit codes, the
  `coretrace/v1` SARIF fingerprint and the `coretrace-1.0` HTTP protocol. Each is documented,
  tested, and changed only as a significant change of [GOVERNANCE.md](GOVERNANCE.md).
- Release `ctrace` 1.0 once they are frozen.

## 2. Keep the analyzers maintained

Goal: analyzers that compile real projects reliably, with fewer false positives at each release.

**Q4 2026**

- Fix the defects the initial static analysis found:
  [coretrace-compiler#204](https://github.com/CoreTrace/coretrace-compiler/issues/204) (stack
  overflow of in-process Clang on worker threads),
  [coretrace-stack-analyzer#214](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/214)
  and [coretrace-concurrency-analyzer#199](https://github.com/CoreTrace/coretrace-concurrency-analyzer/issues/199)
  (`-Werror` replayed from the compilation database).
- Release the stack analyzer from `main`, which unblocks #169.

**Continuously**

- Work down the open false positives and false negatives: 30 bug reports in
  [coretrace-stack-analyzer](https://github.com/CoreTrace/coretrace-stack-analyzer/issues?q=is%3Aissue%20state%3Aopen%20label%3Abug)
  and 32 in
  [coretrace-concurrency-analyzer](https://github.com/CoreTrace/coretrace-concurrency-analyzer/issues?q=is%3Aissue%20state%3Aopen%20label%3Abug)
  on 2026-10-14, each fixed with a regression fixture.
- Release each analyzer when its fixes land, and update `ctrace`'s pins.
- Follow new LLVM versions: the analyzers depend on LLVM/Clang 20 today.

## 3. Windows

Goal: `ctrace` and its analyzers built, tested and released on Windows.

Today, `coretrace-compiler` and `coretrace-gui` build and test on Windows in CI. `ctrace`, the
stack analyzer, the concurrency analyzer and the runtime analyzer do not; the first Windows port
of `ctrace` was reverted in September 2026 (see [Regressions caught](docs/quality/regressions.md),
R3). The order follows the dependencies:

**Q1 2027**

1. Build and test the stack analyzer on Windows
   ([coretrace-stack-analyzer#151](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/151)),
   then the concurrency analyzer.
2. Port `ctrace`'s process backend
   ([#54](https://github.com/CoreTrace/coretrace/issues/54)), with a Windows job among the
   required checks from the start.
3. Publish Windows release archives, verified like the Linux ones.

The runtime analyzer comes after: its instrumentation is specific to each platform.

## 4. Grow the community

Goal: contributors who come back.

**Q4 2026**

- Make the first steps easy: an introduction in the README, issue and pull request templates,
  and a reproducibility test of the setup by someone outside the team.
- Welcome tickets in every repository of level 1, including the analyzers, which have none.
- At least five calls for contributions in November and December, each recorded with its date,
  channel and outcome.
- Review every contribution with GitHub's review tool, and invite contributors who return to
  take on larger issues.

**Q1 2027**

- Measure what the calls brought: visits and clones (`coretrace-governance/traffic`),
  contributions received, contributors who came back. Keep the channels that worked.
- Propose maintainership to contributors who meet the criteria of
  [GOVERNANCE.md](GOVERNANCE.md).

## After the EIP

CoreTrace continues as the open core of the CoreTrace platform, the hosted analysis service.

- The open repositories stay under Apache-2.0 and keep being maintained by the same rules:
  pull requests, required checks, reviews of contributions, decisions in issues.
- The hosted service is developed separately; what it adds and what stays open is set by the
  commitments of [GOVERNANCE.md](GOVERNANCE.md).
- This roadmap is reviewed every quarter, in a pull request that says what was done, what moved
  and why.
- If the team that maintains the project shrinks, these come first, in this order: security
  fixes, reviews of contributions, compatibility with new LLVM versions.
