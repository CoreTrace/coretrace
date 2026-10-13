# Open-source scope

What CoreTrace opens, what it keeps private, where it welcomes contributors, and why.

| | |
|---|---|
| Date | 2026-10-13 |
| Scope | The 25 repositories of the [CoreTrace organization](https://github.com/CoreTrace) |

## Criteria

A repository is public when all three hold:

1. **It is useful outside the team**: someone can run it, reuse it or learn from it.
2. **Its license is clear** (see [Licensing](licensing.md)).
3. **It holds nothing that must stay closed**: secrets, personal data, legal evidence, internal
   evaluation material, brand assets.

Opening the code is not the same as accompanying contributors. The team accompanies
contributors (welcome tickets, contribution calls, onboarding documentation) only where it
develops actively: the product as the acceptance test plan defines it, and its desktop
interface. Elsewhere, the public code is offered as is.

## Four levels

| Level | What it means | Repositories |
|---|---|---:|
| 1. Open, contributions accompanied | Welcome tickets, contribution calls, `CONTRIBUTING.md`, required CI | 5 |
| 2. Open and maintained | Issues and pull requests are welcome and reviewed; no contribution calls | 7 |
| 3. Published as is | Readable and reusable under its license, but not maintained | 5 |
| 4. Private | Not published | 8 |

### 1. Open, contributions accompanied: the product and its interface

| Repository | Role | License |
|---|---|---|
| [`coretrace`](https://github.com/CoreTrace/coretrace) | `ctrace`, the CLI and server that runs the analyzers and merges their reports | Apache-2.0 |
| [`coretrace-stack-analyzer`](https://github.com/CoreTrace/coretrace-stack-analyzer) | Stack, buffer, pointer-escape and initialization analysis on LLVM IR | Apache-2.0 |
| [`coretrace-concurrency-analyzer`](https://github.com/CoreTrace/coretrace-concurrency-analyzer) | Data races, missing joins, lock-order deadlocks | Apache-2.0 |
| [`coretrace-runtime-analyzer`](https://github.com/CoreTrace/coretrace-runtime-analyzer) | Instrumented runs: bounds, allocations, vtables | Apache-2.0 |
| [`coretrace-gui`](https://github.com/CoreTrace/coretrace-gui) | Desktop interface (Tauri) over `ctrace` | Apache-2.0 |

Why these five:

- **The first four are the product.** The acceptance test plan evaluates CoreTrace through
  exactly these four repositories, and one release archive ships them together.
- **The GUI widens who can contribute.** The four others need C++ and LLVM. The GUI is
  TypeScript and web code, where a newcomer can start without knowing compilers. It already
  has three welcome tickets
  ([#81](https://github.com/CoreTrace/coretrace-gui/issues/81),
  [#85](https://github.com/CoreTrace/coretrace-gui/issues/85),
  [#86](https://github.com/CoreTrace/coretrace-gui/issues/86)).
- **Contributors come in through `coretrace`.** Every external contribution so far landed there
  ([#147](https://github.com/CoreTrace/coretrace/pull/147),
  [#165](https://github.com/CoreTrace/coretrace/pull/165),
  [#166](https://github.com/CoreTrace/coretrace/pull/166),
  [#168](https://github.com/CoreTrace/coretrace/pull/168)), and its README build, welcome
  tickets and protected `main` are the entry point.
- **The analyzers follow the same rules**: Apache-2.0, a `CONTRIBUTING.md`, CI on every pull
  request. A contributor who starts in `coretrace` can follow a finding into the analyzer that
  produced it.

### 2. Open and maintained

| Repository | Role | License | Why not level 1 |
|---|---|---|---|
| [`coretrace-compiler`](https://github.com/CoreTrace/coretrace-compiler) | Clang integration library every analyzer compiles through | Apache-2.0 | A change there affects every analyzer: not a first contribution |
| [`coretrace-python-analyzer`](https://github.com/CoreTrace/coretrace-python-analyzer) | Python analysis, bundled in `ctrace` | Apache-2.0 | Joined the product in September 2026; not in the acceptance test plan yet |
| [`coretrace-log`](https://github.com/CoreTrace/coretrace-log) | C++20 logging library | MIT | Small and stable |
| [`coretrace-vscode`](https://github.com/CoreTrace/coretrace-vscode) | VS Code extension | GPL-3.0 | Outside the evaluated product |
| [`coretrace-tool-template`](https://github.com/CoreTrace/coretrace-tool-template) | Scaffold for a new CoreTrace tool | Apache-2.0 | Changes only when the conventions change |
| [`coretrace-ci-consumer-demo`](https://github.com/CoreTrace/coretrace-ci-consumer-demo) | Reference CI integration, used as an external consumer would | none (see actions) | A reference to copy, not a component |
| [`.github`](https://github.com/CoreTrace/.github) | Organization profile | — | Not code |

### 3. Published as is

| Repository | What it is | License | Last change |
|---|---|---|---|
| [`coretrace-TscanCode`](https://github.com/CoreTrace/coretrace-TscanCode) | Fork of Tencent's TscanCode, an optional tool of `ctrace` | GPL-3.0 | 2026-01 |
| [`coretrace-apex`](https://github.com/CoreTrace/coretrace-apex) | PE and ELF binary analysis experiment | GPL-3.0 | 2025-11 |
| [`coretrace-testkit`](https://github.com/CoreTrace/coretrace-testkit) | Python test framework for tools; no repository uses it | GPL-3.0 | 2026-02 |
| [`coretrace-web`](https://github.com/CoreTrace/coretrace-web) | Early web interface, replaced by `coretrace-gui` | none | 2025-07 |
| [`coretrace-qt`](https://github.com/CoreTrace/coretrace-qt) | Qt client, replaced by `coretrace-gui`; archived | GPL-3.0 | 2025-11 |

These stay public: hiding published code would not withdraw it, because forks and copies
remain, but it would break the links to it. Being public keeps the project's history readable.

### 4. Private

| Repository | What it is | Why it stays private |
|---|---|---|
| `ATP` | Acceptance test plan of the EIP: spreadsheet, scripts, datasets | Internal evaluation deliverable |
| `static-analysis-comparison-report` | Benchmark of the stack analyzer against SonarQube | Internal evaluation deliverable |
| `coretrace-governance` | Proof archive of the code and its authorship, license and contributor inventories, traffic records | Legal evidence and personal data (contributor identities) |
| `coretrace-website` | Showcase website | Brand choice: the site presents the project and adds nothing to the tools; Apache-2.0 does not license the name either (section 6) |
| `coretrace-ftrace` | Tracing prototype, abandoned in 2025-06 | Unfinished: publishing it would invite use of code nobody maintains |
| `angr-stack-analyser` | Buffer-overflow detection experiment with angr, 2025-11 | Abandoned experiment |
| `coretrace-fuzz` | Fuzzing experiment; archived, holds only a README | Abandoned experiment |
| `coretrace-update-notifier` | Meant to announce GUI updates; empty, and nothing uses it | Nothing to publish |

## Gaps and actions

1. **`coretrace-ci-consumer-demo` has no license**, while it is meant to be copied into other
   projects' CI. Add Apache-2.0, like the components it uses.
2. **`coretrace-web` is public without a license** and no longer maintained. Archive it; add a
   license only if its reuse is wanted.
3. **Level 3 repositories do not say they are unmaintained.** State it at the top of each
   README, and archive those that will not change.
4. **The level 1 analyzers have no welcome ticket**, while `coretrace` and the GUI have three
   each. Open some before the contribution calls of November.
5. **The GUI lacks two of the level 1 rules.** It has no `CONTRIBUTING.md`, and its CI runs on
   pull requests but no rule requires it to pass. Add both, as in `coretrace`.
6. **`coretrace-update-notifier` is empty.** Delete it, or archive it until the feature exists.
7. **The organization profile does not show these levels**, and its license column is out of
   date (see [Licensing](licensing.md), action 4). Align it with this note.
