<div align="center">

# CoreTrace

**Static and dynamic analysis. Unified diagnostics.**

Analyze C/C++ with `ctrace`, add optional Python analysis, and bring findings<br>
to your terminal, SARIF reports or HTTP workflows.

[**Website**](https://coretrace.fr) · [**Web app**](https://app.coretrace.fr/) · [**Releases**](https://github.com/CoreTrace/coretrace/releases) · [**Configuration**](docs/configuration.md) · [**Contributing**](CONTRIBUTING.md)

[![CI](https://github.com/CoreTrace/coretrace/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/CoreTrace/coretrace/actions/workflows/build.yml)
[![Formatting](https://github.com/CoreTrace/coretrace/actions/workflows/clang-format.yml/badge.svg?branch=main)](https://github.com/CoreTrace/coretrace/actions/workflows/clang-format.yml)
[![License checks](https://github.com/CoreTrace/coretrace/actions/workflows/license-compliance.yml/badge.svg?branch=main)](https://github.com/CoreTrace/coretrace/actions/workflows/license-compliance.yml)
[![Line coverage: 89.56%, measured on 2026-10-04](https://img.shields.io/badge/coverage-89.56%25-brightgreen)](#coverage)

<sub>Coverage snapshot: [2026-10-04 on main](#coverage). CI badges update automatically.</sub>

[![Release](https://img.shields.io/github/v/release/CoreTrace/coretrace)](https://github.com/CoreTrace/coretrace/releases)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-green.svg)](LICENSE)
[![Stars](https://img.shields.io/github/stars/CoreTrace/coretrace?style=flat&logo=github)](https://github.com/CoreTrace/coretrace/stargazers)
[![Community: issues](https://img.shields.io/badge/community-issues-blue?logo=github)](https://github.com/CoreTrace/coretrace/issues)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white)](#build)
[![LLVM 20](https://img.shields.io/badge/LLVM-20-8F0000?logo=llvm&logoColor=white)](#build)
[![Clang 20](https://img.shields.io/badge/Clang-20-00599C?logo=llvm&logoColor=white)](#build)

</div>

---

[Quick start](#quick-start) · [Build](#build) · [Analyzers](#analyzers) · [Releases](#releases) · [Configuration](#configuration) · [SARIF](#sarif-output) · [Exit codes](#exit-codes) · [HTTP API](#server-mode) · [Development](#development)

## Overview

`ctrace` finds bugs in C and C++ code before it runs: stack buffer overflows, data races,
deadlocks, uninitialized reads, memory errors at run time. It runs several analyzers on your
sources, merges what they find into one report (text, JSON or SARIF), and returns an exit code
that can fail a CI pipeline. It also checks Python projects.

It is for C and C++ developers and the CI pipelines of their projects. Part of
[CoreTrace](https://github.com/CoreTrace); see the [roadmap](ROADMAP.md) for where it is going.

| Capability | What you get |
| :--- | :--- |
| **Code analysis** | Static and dynamic analysis for C/C++, plus an optional Python analyzer. |
| **Unified findings** | Consistent diagnostics across tools, with SARIF 2.1.0 export. |
| **Workflow integration** | A command-line interface, configurable CI failure thresholds and an HTTP API. |

## Quick start

On Linux (x86-64 or arm64, glibc 2.34 or newer), the release archive needs neither a compiler
nor LLVM; only the C library headers (`libc6-dev` on Debian and Ubuntu, `glibc-devel` on RHEL).
Take the archive of the [latest release](https://github.com/CoreTrace/coretrace/releases/latest)
for your machine, for example:

```bash
curl -fsSLO https://github.com/CoreTrace/coretrace/releases/download/v0.85.0/coretrace-v0.85.0-linux-amd64.tar.gz
tar -xzf coretrace-v0.85.0-linux-amd64.tar.gz
```

Save this as `demo.c`:

```c
int main(void)
{
    int values[4];
    for (int i = 0; i <= 4; i++)
        values[i] = i;
    return 0;
}
```

and analyze it:

```bash
./coretrace-v0.85.0-linux-amd64/bin/ctrace --input demo.c \
    --invoke ctrace_stack_analyzer,coretrace-concurrency-analyzer --fail-on warning
```

```
at line 5, column 19
[ !!Warn ] potential stack buffer overflow on variable 'values' (size 4)
     ↳ alias path: values
     ↳ index variable may go up to 4 (array last valid index: 3)
     ↳ (this is a write access)
```

The exit code is 2: a finding at or above `--fail-on`. On macOS (Apple silicon, macOS 14 or
newer), take the disk image of the release instead ([Releases](#releases)). To change the code,
[build from source](#build).

Export the findings as SARIF for your CI workflow, a report that GitHub Code Scanning reads:

```bash
./coretrace-v0.85.0-linux-amd64/bin/ctrace --input demo.c --invoke ctrace_stack_analyzer \
  --sarif-format --report-file ctrace.sarif
```

### Use a configuration file

Save the equivalent settings as `coretrace.json` at your project root, adjusting `files.input`
to match your source files:

```json
{
  "schema_version": 1,
  "analysis": {
    "invoke": ["ctrace_stack_analyzer"]
  },
  "files": {
    "input": ["src/main.c"]
  },
  "output": {
    "sarif_format": true,
    "report_file": "ctrace.sarif"
  }
}
```

Pass the file to the orchestrator from your project root:

```bash
ctrace --config coretrace.json
```

Relative input paths are resolved from the configuration file's directory. Omitted settings
keep their defaults, and command-line options override the file's values.
See the [configuration reference](docs/configuration.md) for all available settings.

## Build

Builds from source need Git, CMake 3.28 or newer, Ninja, a C++20 compiler whose standard library
provides `<format>` (GCC 13 or newer, or Clang with libstdc++ 13 or libc++), and the LLVM 20 and
Clang 20 development packages. The test suite also runs cppcheck. The build is tested on Ubuntu
24.04 and macOS 14, and was checked on Ubuntu 22.04. Use matching
LLVM and Clang installations: `LLVM_DIR` and `Clang_DIR` select their CMake packages, while
`CLANG_EXECUTABLE` selects the Clang 20 executable whose resource headers are bundled with
`ctrace`. These paths are build-time settings; an installed or released `ctrace` does not need
LLVM or Clang on the machine where it runs.

### Configure

<details>
<summary><strong>Ubuntu 24.04 · LLVM/Clang 20</strong></summary>

On Ubuntu 24.04, install the build tools and the same LLVM/Clang 20 packages as CI, then
configure from the repository root:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git curl ca-certificates gnupg cppcheck
sudo ./scripts/ci/install-llvm-apt.sh 20
cmake -S . -B build -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm \
  -DClang_DIR=/usr/lib/llvm-20/lib/cmake/clang \
  -DCLANG_EXECUTABLE=/usr/bin/clang-20 \
  -DENABLE_PYTHON_ANALYZER=OFF
```

</details>

<details>
<summary><strong>Ubuntu 22.04 · GCC 13 · LLVM/Clang 20</strong></summary>

On Ubuntu 22.04, the system's GCC 11 lacks `<format>` and its CMake is 3.22: take GCC 13 from the
toolchain PPA, as the release build does, and a CMake release from Kitware. The PPA also updates
the system's `libstdc++6` and `libgcc-s1`.

```bash
sudo apt-get update
sudo apt-get install -y build-essential ninja-build git curl ca-certificates gnupg cppcheck software-properties-common
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get install -y g++-13
sudo ./scripts/ci/install-llvm-apt.sh 20
curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-$(uname -m).tar.gz | tar -xz -C "$HOME"
export PATH="$HOME/cmake-3.31.6-linux-$(uname -m)/bin:$PATH"
CC=gcc-13 CXX=g++-13 cmake -S . -B build -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm \
  -DClang_DIR=/usr/lib/llvm-20/lib/cmake/clang \
  -DCLANG_EXECUTABLE=/usr/bin/clang-20 \
  -DENABLE_PYTHON_ANALYZER=OFF
```

</details>

<details>
<summary><strong>macOS · Homebrew LLVM/Clang 20</strong></summary>

On macOS, use Homebrew's versioned LLVM formula:

```bash
brew install cmake ninja llvm@20 cppcheck
cmake -S . -B build -G Ninja \
  -DLLVM_DIR="$(brew --prefix llvm@20)/lib/cmake/llvm" \
  -DClang_DIR="$(brew --prefix llvm@20)/lib/cmake/clang" \
  -DCLANG_EXECUTABLE="$(brew --prefix llvm@20)/bin/clang" \
  -DENABLE_PYTHON_ANALYZER=OFF
```

</details>

<details>
<summary><strong>Optional Python analyzer · Python 3.11+</strong></summary>

Python is optional: the configuration above builds `ctrace` without the Python analyzer, so
it needs no Python virtual environment or Nuitka. `ENABLE_PYTHON_ANALYZER` defaults to `OFF`;
passing it explicitly also turns it off when reusing a build directory configured with Python.
To include [coretrace-python-analyzer](https://github.com/CoreTrace/coretrace-python-analyzer),
install Python 3.11 or newer (CI uses 3.12): `brew install python@3.12` on macOS, or
`sudo apt-get install -y patchelf python3.12-venv` on Ubuntu 24.04. Use the **same interpreter**
for Nuitka and `Python3_EXECUTABLE`. Run the following commands after the initial CMake
configuration above:

```bash
python3.12 -m venv .venv
.venv/bin/python -m pip install nuitka==4.2.2
cmake -S . -B build \
  -DENABLE_PYTHON_ANALYZER=ON \
  -DPython3_EXECUTABLE="$PWD/.venv/bin/python"
```

Nuitka 4.2.2 is the version tested in CI. CMake downloads the Python analyzer's pinned
`v0.12.0` tag by default; set `-DCORETRACE_PYTHON_ANALYZER_TAG=<tag>` to select another release,
or `-DCORETRACE_PYTHON_ANALYZER_SOURCE_DIR=<checkout>` to build a local checkout. The build
needs Python and a C compiler, but the resulting standalone analyzer includes its Python runtime.
It is staged in `build/libexec/coretrace/` and installed to
`<prefix>/libexec/coretrace/`; users of the built or installed `ctrace` need no Python installation.

</details>

### Build and verify

Build and verify the selected configuration:

```bash
cmake --build build --parallel 4
./build/ctrace --help
ctest --test-dir build --output-on-failure
```

## Analyzers

Select one or more analyzers with `--invoke`, use `--static` for the static tool set, or `--dyn`
for runtime analysis. Each tool receives the inputs of its language.

| Analyzer | Analysis | Integration |
| :--- | :--- | :--- |
| [CoreTrace Stack Analyzer](#coretrace-stack-analyzer) | Static C/C++ | Built into `ctrace` |
| [CoreTrace Concurrency Analyzer](#coretrace-concurrency-analyzer) | Static C/C++ | Built into `ctrace` |
| [CoreTrace Runtime Analyzer](#coretrace-runtime-analyzer) | Dynamic C/C++ | Optional executable |
| [CoreTrace Python Analyzer](#coretrace-python-analyzer) | Static Python | Optional executable |
| [Cppcheck](#cppcheck) | Static C/C++ | External executable |
| [Flawfinder](#flawfinder) | Static C/C++ | External executable |
| [TscanCode](#tscancode) | Static C/C++ | External executable |
| [IKOS](#ikos) | Static C/C++ | External executable |

External executables are resolved from `tools.<name>.path`, then the copy bundled with
`ctrace`, then `PATH`. `--static` also selects the external static tools, so they must be
installed. For C/C++ inputs, `--static` includes IKOS. Since CoreTrace does not interpret its
findings, these runs end incomplete (exit code 3). Use `--invoke` to select the tools available
on your machine and gate on a verdict. See [tool configuration](docs/configuration.md#tools) for
executable paths, additional arguments and timeouts.

### CoreTrace Stack Analyzer

[coretrace-stack-analyzer](https://github.com/CoreTrace/coretrace-stack-analyzer) analyzes C/C++
through LLVM IR to report stack usage, recursion risks, buffer errors, escaping stack addresses
and resource-lifetime issues. It is linked into `ctrace` and compiles inputs in process;
no separate analyzer executable is needed.

```bash
ctrace --input src/main.c --invoke ctrace_stack_analyzer
```

The orchestrator's tool name is `ctrace_stack_analyzer`. Configure its analysis profile,
models, stack limit and SMT settings in the
[`stack_analyzer` section](docs/configuration.md#stack_analyzer). Use `--compile-commands`
to supply your project's compilation database.

<a id="concurrency"></a>

### CoreTrace Concurrency Analyzer

[coretrace-concurrency-analyzer](https://github.com/CoreTrace/coretrace-concurrency-analyzer) is
linked into `ctrace` like the stack analyzer, and compiles in process too
(`--invoke coretrace-concurrency-analyzer`, and part of `--static`). It reports data races on
shared globals, lock-order deadlocks, missing joins, condition waits without a predicate, thread
arguments escaping their frame, unsafe signal handlers and weak publication ordering, each with
its CWE and the other location involved (the conflicting access of a race) as a note and a SARIF
related location. With `--compile-commands`, the inputs are analyzed as one program, each built
as the database says, so a thread started in one file is related to its body in another.
`tools.coretrace-concurrency-analyzer.rules` selects its rules.

```bash
ctrace --input src/main.c --invoke coretrace-concurrency-analyzer
```

Configure rule selection and project analysis limits under
[`tools.coretrace-concurrency-analyzer`](docs/configuration.md#tools).

<a id="runtime"></a>

### CoreTrace Runtime Analyzer

`--dyn` runs [coretrace-runtime-analyzer](https://github.com/CoreTrace/coretrace-runtime-analyzer)
on each C/C++ input, which must be a whole program: it is built with CoreTrace instrumentation
and run, and the memory errors that happen (heap and stack overflows, uses after free, double
frees, leaks) join the other tools' findings, merged with a static finding at the same line and
CWE. The Linux release archives ship the tool in `libexec/coretrace/coretrace-runtime-analyzer/`
(`-DENABLE_RUNTIME_ANALYZER=ON` downloads its pinned release and checks it against the SHA-256
recorded in `cmake/runtimeAnalyzer.cmake`). Its libraries and headers identical to those of
`lib/` are stored once, as hard links, which keeps the archive about 75 MiB smaller. Linking an
instrumented program needs a C++ build
environment (`g++` on Debian and Ubuntu, `gcc-c++` on RHEL). The program runs with `ctrace`'s
privileges, in a separate process but not in a sandbox, so the HTTP server refuses dynamic
analysis unless it is started with `--serve-allow-dynamic`.

```bash
ctrace --input src/main.c --invoke coretrace-runtime-analyzer
```

Use [`tools.coretrace-runtime-analyzer`](docs/configuration.md#tools) to configure its executable,
execution timeout and additional compiler arguments. `--dyn` selects the same analyzer.

<a id="python"></a>

### CoreTrace Python Analyzer

[coretrace-python-analyzer](https://github.com/CoreTrace/coretrace-python-analyzer) checks Python
projects for injection vulnerabilities, dangerous API usage, exposed secrets and vulnerable
dependencies. It follows data across functions and modules without executing the project.

Enable its standalone build as described in [Build](#build), or install the package from PyPI
and make its command available through `PATH` or `tools.coretrace-python-analyzer.path`.
The standalone bundle includes Python; installing from PyPI requires Python 3.11 or newer.

[![Python analyzer on PyPI](https://img.shields.io/pypi/v/coretrace-python-analyzer?label=Python%20analyzer&logo=pypi&logoColor=white)](https://pypi.org/project/coretrace-python-analyzer/)
[![Python versions](https://img.shields.io/pypi/pyversions/coretrace-python-analyzer?logo=python&logoColor=white)](https://pypi.org/project/coretrace-python-analyzer/)

With the Python analyzer enabled, for example:

```bash
./build/ctrace --input app.py --invoke coretrace-python-analyzer
```

Each input goes to the tools of its language: `.py` files to `coretrace-python-analyzer`, C
and C++ files to the others. The Python analyzer checks the whole project a file belongs to,
so it follows data across modules, but it only reports findings located in the files given to
`--input`, plus those about the project itself, such as a vulnerable version pinned in
`requirements.txt`. The project root is the nearest directory above the file that holds
`pyproject.toml`, `setup.py`, `setup.cfg` or `.git`, else the file's own directory.

Configure its executable, additional arguments and timeout under
[`tools.coretrace-python-analyzer`](docs/configuration.md#tools).

### Cppcheck

[Cppcheck](https://github.com/cppcheck-opensource/cppcheck) provides general-purpose static
analysis for C/C++, including memory errors, null-pointer issues and coding mistakes.
CoreTrace enables its warning, style, performance and portability checks, forwards project
include directories and defines, and collects findings from all C/C++ inputs in one run.

Install `cppcheck` separately and make it available through `PATH` or `tools.cppcheck.path`.

```bash
ctrace --input src/main.c --invoke cppcheck
```

Use [`tools.cppcheck`](docs/configuration.md#tools) for extra arguments, suppressions and the
tool's worker count (`jobs`). Shared project settings belong in
[`build.include_dirs` and `build.defines`](docs/configuration.md#build).

### Flawfinder

[Flawfinder](https://github.com/david-a-wheeler/flawfinder) scans C/C++ source for potentially
unsafe function calls and security-sensitive API usage. CoreTrace reads its SARIF output
and includes the findings in the unified diagnostics.

Install Flawfinder **2.0.16 or newer**, the first version with SARIF output, and expose its
executable through `PATH` or `tools.flawfinder.path`.

```bash
ctrace --input src/main.c --invoke flawfinder
```

Configure extra arguments and the timeout under
[`tools.flawfinder`](docs/configuration.md#tools), preserving the SARIF output expected by
the integration.

### TscanCode

[TscanCode](https://github.com/CoreTrace/coretrace-TscanCode) checks C/C++ code for defects such
as null-pointer dereferences. CoreTrace runs it with `--enable=all` over the input file list
and converts its text diagnostics into the common reporting format.

Build or install the executable from the CoreTrace fork, then make `tscancode` available
through `PATH` or `tools.tscancode.path`.

```bash
ctrace --input src/main.c --invoke tscancode
```

Configure its executable, additional arguments and timeout under
[`tools.tscancode`](docs/configuration.md#tools).

### IKOS

[IKOS](https://github.com/NASA-SW-VnV/ikos) is a static C/C++ analyzer based on abstract
interpretation. CoreTrace invokes it once per source file and forwards the configured entry
points. Install IKOS and its required toolchain using the upstream instructions, then make
`ikos` available through `PATH` or `tools.ikos.path`.

```bash
ctrace --input src/main.c --invoke ikos
```

Set entry points with `--entry-points` or `files.entry_points`. Executable paths, additional
arguments and timeouts are configured under [`tools.ikos`](docs/configuration.md#tools).

**Current integration limit:** IKOS output is not yet converted into CoreTrace diagnostics.
It is displayed as raw output in console mode; its findings are excluded from the merged SARIF
report, diagnostic counts and the `--fail-on` findings threshold. Whenever IKOS runs, the
analysis ends incomplete (exit code 3), because CoreTrace does not interpret its findings.

## Releases

The Linux release archives (amd64, arm64) run on any distribution with glibc 2.34 or newer:
RHEL 9 and its rebuilds (Rocky, Alma), Amazon Linux 2023, Ubuntu 22.04+, Debian 12+, Fedora
35+. ctrace is built on Ubuntu 22.04 and carries its own libstdc++; the Python analyzer is
built on Rocky Linux 9. Each archive is unpacked and run on bare Rocky Linux 9, Ubuntu 22.04,
Debian 12 and Ubuntu 24.04 images, with no compiler or LLVM, before it is published.

The macOS disk image runs on Apple silicon with macOS 14 or newer. It holds one folder laid out
as the Linux archive, with the Homebrew libraries `ctrace` and the Python analyzer load (LLVM and
Clang, Python and OpenSSL...): copy the folder anywhere, Homebrew is not needed. Before it is
published, the image is mounted, its folder copied out, and run with Homebrew moved away. Its
third-party libraries are listed in `scripts/release/third-party-macos.txt`, and their licenses,
with each Homebrew formula's version, are under `licenses/homebrew/`.

Each archive carries the license of every third-party component it ships. `licenses/` holds
those of the code compiled into `ctrace` and of the Ubuntu libraries in `lib/`, with each
package's exact version. Each bundled analyzer holds its own under `libexec/coretrace/`.
`scripts/release/third-party.txt` lists what ships and where its license is. The packaging, and
the verification on each distribution, fail on a library or an executable it does not list.

Analyzing C and C++ needs no LLVM or clang installation either: Clang's own headers ship in
`lib/clang/<version>/include` next to `bin/ctrace`, and the stack analyzer compiles in process.
Only the C library's headers come from the system (`libc6-dev` on Debian and Ubuntu,
`glibc-devel` on RHEL, the SDK of the Command Line Tools on macOS: `xcode-select --install`), as
for any C build. `CT_CLANG` still selects another clang when set.

### Verify a release

Each release attaches the Linux archives and the macOS disk image with their `.sha256`, and a
build provenance attestation for each: a signed statement that this repository's release
workflow built it from the tagged commit. Check a downloaded archive with the GitHub CLI:

```bash
gh attestation verify coretrace-<version>-linux-amd64.tar.gz -R CoreTrace/coretrace
```

The disk image is signed ad hoc, not with an Apple Developer ID, and is not notarized: macOS
refuses to run a `ctrace` that a browser downloaded. Download it with `curl`, which does not mark
the file, or remove the mark before opening it:

```bash
curl -fsSLO https://github.com/CoreTrace/coretrace/releases/download/<version>/coretrace-<version>-macos-arm64.dmg
# or, after a download in a browser:
xattr -d com.apple.quarantine coretrace-<version>-macos-arm64.dmg
```

Every dependency the build fetches is pinned to a version tag or a commit, and every download
to its SHA-256 (`scripts/release/check-pinned-dependencies.sh`, run by `ctest`).

<a id="argument"></a>

## Command-line options

The option list is generated from the binary and is the single source of truth:

```bash
ctrace --help
```

## Configuration

- Canonical default config: [`config/tool-config.json`](config/tool-config.json)
- Full schema and semantics: [Configuration reference](docs/configuration.md)
- Precedence: built-in defaults < config file < CLI

For example, from the repository root with a local build:

```bash
./build/ctrace --input tests/EmptyForStatement.cc --entry-points=main --verbose \
    --invoke cppcheck,ctrace_stack_analyzer
```

## SARIF output

`--sarif-format` reports the whole run as **one** SARIF 2.1.0 log, whatever tools ran: one
`run` per tool, rules collected per tool, a `coretrace/v1` partial fingerprint per result for
stable alerts across runs, and the CWE as a result property. The document is printed to
stdout and written to `--report-file`, ready for `github/codeql-action/upload-sarif`.

```bash
ctrace --sarif-format --invoke cppcheck,flawfinder,ctrace_stack_analyzer \
    --input src/main.c --report-file ctrace.sarif
```

Every tool spells a file the same way: an input as it was typed on the command line, any
other file relative to the working directory when below it, else absolute. The locations and
the fingerprints of a run are therefore the same whichever tool reports the finding and on
whichever machine the run happens, when it runs from the repository root.

A rule is described as its tool describes it, so that code scanning can explain a finding: its
title as `shortDescription`, what it detects as `fullDescription`, and its documentation link as
`helpUri`. Today flawfinder gives a title and a CWE link. The concurrency, stack, Python and
runtime analyzers do not describe their rules yet
([coretrace-concurrency-analyzer#214](https://github.com/CoreTrace/coretrace-concurrency-analyzer/issues/214),
[coretrace-stack-analyzer#242](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/242),
[coretrace-python-analyzer#270](https://github.com/CoreTrace/coretrace-python-analyzer/issues/270),
[coretrace-runtime-analyzer#29](https://github.com/CoreTrace/coretrace-runtime-analyzer/issues/29)):
their rules keep their id alone.

A finding keeps the other places it involves and how sure its tool is, whichever tool reports
it: the related locations (`relatedLocations`, such as the other access of a data race or the
allocation of a heap overflow), which the message links to so that code scanning shows them as
links, and the confidence (`properties.confidence`, `low`, `medium` or `high`). The stack
analyzer's confidence, a number, is not carried yet
([coretrace-stack-analyzer#244](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/244)).

A weakness reported by several tools, same file, same line and same CWE, is one result: the
most severe report is kept in its tool's run, and its `alsoReportedBy` property names the other
tools and rules. Reports without a CWE cannot be matched and are all kept; the console output
and the counters still show every tool's report.

## Exit codes

`ctrace` is meant to gate a CI pipeline, so its exit code is a verdict on the run:

| Code | Meaning |
|---|---|
| `0` | Analysis complete, no finding at or above `--fail-on` |
| `1` | Usage or configuration error, including a run with no analysis selected (`--static`, `--dyn` or `--invoke`) |
| `2` | Findings at or above `--fail-on` (default: `error`) |
| `3` | Analysis incomplete: a tool could not be started, crashed or exited abnormally, or `ctrace` could not read its findings |

`--fail-on error|warning|none` (config: `analysis.fail_on`) sets the threshold. `none` never
fails on findings but still returns `3` on an incomplete analysis. Code `3` takes precedence over
`2`.

`ctrace` cannot read the findings of `ikos`, of `flawfinder` when it prints no SARIF, or of
`cppcheck` and `tscancode` when a line of their output cites a source location (`file:line`, in
any format and any file, a header included) but does not parse. Such a run exits `3` and names
the tool, since its counts would be a guess; the findings read still count, and the tool's raw
output is shown. For C/C++ inputs, `--static` includes `ikos`, so these runs end incomplete: to
gate on a verdict, pick the tools with `--invoke`.

Interrupting a run (Ctrl+C, SIGINT or SIGTERM) stops the external tools it runs, with what they
started, and removes their run directories. The run gives no report, and `ctrace` ends by the
signal it received: the shell shows `130` or `143`. No tool starts after the interrupt. The
stack and concurrency analyzers are linked into `ctrace` and cannot be stopped, so `ctrace` waits
for one already running; interrupt again to quit at once.

```bash
ctrace --fail-on warning --input src/main.c --invoke cppcheck,ctrace_stack_analyzer
```

## Server mode

### Start the server

Start the HTTP server from the repository root, where the request below finds its input:

```bash
./build/ctrace --ipc serve --serve-host 127.0.0.1 --serve-port 8080 --shutdown-token mytoken
```

If the address and port are already in use, the server exits with code 1. A host name is
resolved once and only its first address is bound (`::1` for `localhost` on macOS), which the
log gives: connect to that address, or use the same name. On macOS, a server on `0.0.0.0` and
another on `127.0.0.1` can still share a port; details in
[Input validation](docs/quality/input-validation.md#6-http-server).

<details>
<summary><strong>Request example and response fields</strong></summary>

Send a request:

```bash
curl -X POST http://127.0.0.1:8080/api \
  -H "Content-Type: application/json" \
  -d '{
    "proto": "coretrace-1.0",
    "id": 1,
    "type": "request",
    "method": "run_analysis",
    "params": {
      "input": ["./tests/buffer_overflow.cc"],
      "entry_points": ["main"],
      "static_analysis": false,
      "dynamic_analysis": false,
      "invoke": ["flawfinder"],
      "sarif_format": true,
      "report_file": "ctrace.sarif",
      "output_file": "ctrace.out",
      "ipc": "serve",
      "ipc_path": "/tmp/coretrace_ipc",
      "jobs": 1,
      "verbose": true
    }
  }'
```

Response notes:

- `status` is `ok` or `error`. An error carries `error.code` and `error.message`, and the HTTP
  status tells its kind: 400 for an invalid request (malformed JSON, a `proto` other than
  `coretrace-1.0`, an unknown method or parameter, a wrong value), 403 for a refused one
  (`DynamicAnalysisDisabled`), 500 for a server failure (`InternalError`, details only in the
  server's log), such as an IPC socket nobody listens on. Bytes that are not UTF-8, in the
  request or in a tool's output, are replaced by U+FFFD in the answer.
- `result.diagnostics` lists every finding in one model (`tool`, `rule_id`, `file`, `line`, `column`, `severity`, `message`, `cwe`), whatever tool produced it; `result.diagnostics_summary_total` counts them by severity.
- `result.sarif` is the merged SARIF log when `sarif_format` is true (also written to `report_file`).
- `result.gate` carries the verdict the CLI would exit with: `fail_on`, `exit_code` (see [Exit codes](#exit-codes)), `failed_tools` and `unread_tools` (the tools that ran but whose findings CoreTrace could not read).
- `result.uninterpreted_tools` names the tools whose output CoreTrace could not interpret: their findings are shown in `result.outputs` but are not counted.
- `result.outputs` groups tool output by tool name.
- Each output entry has `stream` and `message`. If a tool emits JSON, `message` is returned as a JSON object.

</details>

### Shutdown

Shutdown the server (HTTP request):

```bash
curl -i -X POST http://127.0.0.1:8080/shutdown \
  -H "Authorization: Bearer mytoken"
```

Alternative header:

```bash
curl -i -X POST http://127.0.0.1:8080/shutdown \
  -H "X-Admin-Token: mytoken"
```

The server responds with `202 Accepted` and stops accepting new requests while allowing in-flight requests to finish
(up to `--shutdown-timeout-ms` if configured). SIGINT and SIGTERM start the same shutdown, after
which the server ends by the signal; a second one ends it at once.

Each `run_analysis` request gets the same shipped defaults as the CLI: the stack analyzer
models and the bundled tools found next to the server's `ctrace` binary fill whatever the
request and its `config` file leave empty.

## Development

### Tests

```bash
ctest --test-dir build                  # every test
ctest --test-dir build -L unit          # one component, in process
ctest --test-dir build -L integration   # ctrace, a server, a socket peer or a real tool
```

### Coverage

The coverage badge shows the combined line coverage measured by the
[successful CI run on 2026-10-04](https://github.com/CoreTrace/coretrace/actions/runs/37216001843)
for commit [`42ae800`](https://github.com/CoreTrace/coretrace/commit/42ae8007c9528e95230ad8f2d44e4a38c60ad33a)
on `main`. This is a dated snapshot; the percentage is not updated automatically.

| Tests | Lines | Functions | Branches |
| :--- | ---: | ---: | ---: |
| Unit | 64.58% | 73.11% | 62.10% |
| Integration | 79.36% | 91.90% | 66.54% |
| **Combined** | **89.56%** | **96.09%** | **81.08%** |

The values come from `summary.md` in that run's `coverage-report` artifact. The coverage job
reports measurements without enforcing a minimum coverage threshold.

Coverage of CoreTrace's own code (`src/`, `include/`, `main.cpp`) needs a Clang build with
`-DCORETRACE_COVERAGE=ON`. `scripts/ci/coverage.sh <build> <output>` then runs the tests and
writes a report for the unit tests, the integration tests and both, with an HTML report in
`<output>/html`. CI publishes the same report as the `coverage-report` artifact. On Linux, the
build links Clang's profile runtime (`libclang-rt-20-dev` from apt.llvm.org), and `LLVM_PROFDATA`
and `LLVM_COV` name the tools of your Clang (`/usr/lib/llvm-20/bin/llvm-profdata`, `llvm-cov`).

### Code style (clang-format)

- **Formatter:** `clang-format` 20, matching CI.
- **Format locally:** `./scripts/format.sh`
- **Check formatting:** `./scripts/format-check.sh`
- **CMake targets:** `cmake --build build --target format` or `--target format-check`
- **CI:** the `clang-format` workflow fails if a source file is not formatted.

### Debug

Enable a sanitizer when configuring CMake:

```bash
-DUSE_THREAD_SANITIZER=ON
```

or

```bash
-DUSE_ADDRESS_SANITIZER=ON
```

> **Warning:** You cannot use `-DUSE_THREAD_SANITIZER=ON` and `-DUSE_ADDRESS_SANITIZER=ON` at the same time.

### Version

```bash
./build/ctrace --version
```

Version resolution is centralized at configure time:

- release builds pass an explicit version string from CI, so published binaries print the exact GitHub release tag
- local Git builds resolve the version from `git describe --tags --dirty --always --match 'v*'`
- if Git metadata is unavailable, the build falls back to `dev`

You can also override the embedded version manually:

```bash
cmake -S . -B build -DCORETRACE_VERSION_OVERRIDE=v0.74.0
```

### Mangle/Demangle API

<details>
<summary><strong>View the C++ API examples</strong></summary>

```c++
bool hasMangled = ctrace_tools::mangle::isMangled(entry_points);

std::cout << "Is mangled : " << hasMangled << std::endl;
if (hasMangled)
    std::cout << abi::__cxa_demangle(entry_points.c_str(), 0, 0, &status) << std::endl;

std::vector<std::string> params1 = {};
std::string mangled1 = ctrace_tools::mangle::mangleFunction("", "single_compute()", params1);
std::cout << "Mangled single_compute(): " << mangled1 << "\n";
std::cout << abi::__cxa_demangle(mangled1.c_str(), 0, 0, &status) << std::endl;

// Example 2 : with namespace
std::vector<std::string> params2 = {"std::string", "int"};
std::string mangled2 = ctrace_tools::mangle::mangleFunction("math", "compute", params2);
std::cout << "Mangled math::compute(std::string, int): " << mangled2 << "\n";

// Example 3 : without parameters with namespace
std::vector<std::string> params3;
std::string mangled3 = ctrace_tools::mangle::mangleFunction("utils", "init", params3);
std::cout << "Mangled utils::init(): " << mangled3 << "\n";

```

</details>

## Contributing

Bug reports, fixes, tests and documentation improvements are welcome.
Start with an issue labelled [`good first issue`](https://github.com/CoreTrace/coretrace/issues?q=is%3Aissue%20state%3Aopen%20label%3A%22good%20first%20issue%22) or browse the [open issues](https://github.com/CoreTrace/coretrace/issues), and read the [contribution guide](CONTRIBUTING.md): a maintainer answers within three business days.
For security concerns, follow the [private reporting process](SECURITY.md).

### Repository documents

| Document | Purpose |
| :--- | :--- |
| [Contributing](CONTRIBUTING.md) | Development setup and contribution guidelines. |
| [Code of conduct](CODE_OF_CONDUCT.md) | Community expectations. |
| [Authors](AUTHORS.md) | Project contributors. |
| [Changelog](CHANGELOG.md) | Release history. |
| [Security](SECURITY.md) | Vulnerability reporting. |
| [Governance](GOVERNANCE.md) | Roles and decision-making. |
| [Roadmap](ROADMAP.md) | Planned work, by priority. |
| [License](LICENSE) | Apache License 2.0. |
