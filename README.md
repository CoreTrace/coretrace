# ctrace

### Repository documents

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [AUTHORS.md](AUTHORS.md)
- [CHANGELOG.md](CHANGELOG.md)
- [LICENSE](LICENSE)
- [SECURITY.md](SECURITY.md)

### BUILD

Builds from source need Git, CMake 3.28 or newer, Ninja, a C++20 compiler, and the LLVM 20 and
Clang 20 development packages. The build is tested on Ubuntu 24.04 and macOS 14. Use matching
LLVM and Clang installations: `LLVM_DIR` and `Clang_DIR` select their CMake packages, while
`CLANG_EXECUTABLE` selects the Clang 20 executable whose resource headers are bundled with
`ctrace`. These paths are build-time settings; an installed or released `ctrace` does not need
LLVM or Clang on the machine where it runs.

On Ubuntu 24.04, install the build tools and the same LLVM/Clang 20 packages as CI, then
configure from the repository root:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git curl ca-certificates gnupg
sudo ./scripts/ci/install-llvm-apt.sh 20
cmake -S . -B build -G Ninja \
  -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm \
  -DClang_DIR=/usr/lib/llvm-20/lib/cmake/clang \
  -DCLANG_EXECUTABLE=/usr/bin/clang-20 \
  -DENABLE_PYTHON_ANALYZER=OFF
```

On macOS, use Homebrew's versioned LLVM formula:

```bash
brew install cmake ninja llvm@20
cmake -S . -B build -G Ninja \
  -DLLVM_DIR="$(brew --prefix llvm@20)/lib/cmake/llvm" \
  -DClang_DIR="$(brew --prefix llvm@20)/lib/cmake/clang" \
  -DCLANG_EXECUTABLE="$(brew --prefix llvm@20)/bin/clang" \
  -DENABLE_PYTHON_ANALYZER=OFF
```

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

Build and verify the selected configuration:

```bash
cmake --build build --parallel 4
./build/ctrace --help
ctest --test-dir build --output-on-failure
```

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

The Linux release archives (amd64, arm64) run on any distribution with glibc 2.34 or newer:
RHEL 9 and its rebuilds (Rocky, Alma), Amazon Linux 2023, Ubuntu 22.04+, Debian 12+, Fedora
35+. ctrace is built on Ubuntu 22.04 and carries its own libstdc++; the Python analyzer is
built on Rocky Linux 9. Each archive is unpacked and run on bare Rocky Linux 9, Ubuntu 22.04,
Debian 12 and Ubuntu 24.04 images, with no compiler or LLVM, before it is published.

Each archive carries the license of every third-party component it ships. `licenses/` holds
those of the code compiled into `ctrace` and of the Ubuntu libraries in `lib/`, with each
package's exact version. Each bundled analyzer holds its own under `libexec/coretrace/`.
`scripts/release/third-party.txt` lists what ships and where its license is. The packaging, and
the verification on each distribution, fail on a library or an executable it does not list.

Analyzing C and C++ needs no LLVM or clang installation either: Clang's own headers ship in
`lib/clang/<version>/include` next to `bin/ctrace`, and the stack analyzer compiles in process.
Only the C library's headers come from the system (`libc6-dev` on Debian and Ubuntu,
`glibc-devel` on RHEL), as for any C build. `CT_CLANG` still selects another clang when set.

[coretrace-concurrency-analyzer](https://github.com/CoreTrace/coretrace-concurrency-analyzer) is
linked into `ctrace` like the stack analyzer, and compiles in process too
(`--invoke coretrace-concurrency-analyzer`, and part of `--static`). It reports data races on
shared globals, lock-order deadlocks, missing joins, condition waits without a predicate, thread
arguments escaping their frame, unsafe signal handlers and weak publication ordering, each with
its CWE and the other location involved (the conflicting access of a race) as a note and a SARIF
related location. With `--compile-commands`, the inputs are analyzed as one program, each built
as the database says, so a thread started in one file is related to its body in another.
`tools.coretrace-concurrency-analyzer.rules` selects its rules.

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

### RELEASES

Each release attaches the Linux archives with their `.sha256`, and a build provenance
attestation for each archive: a signed statement that this repository's release workflow built
it from the tagged commit. Check a downloaded archive with the GitHub CLI:

```bash
gh attestation verify coretrace-<version>-linux-amd64.tar.gz -R CoreTrace/coretrace
```

Every dependency the build fetches is pinned to a version tag or a commit, and every download
to its SHA-256 (`scripts/release/check-pinned-dependencies.sh`, run by `ctest`).

### TESTS

```bash
ctest --test-dir build                  # every test
ctest --test-dir build -L unit          # one component, in process
ctest --test-dir build -L integration   # ctrace, a server, a socket peer or a real tool
```

Coverage of CoreTrace's own code (`src/`, `include/`, `main.cpp`) needs a Clang build with
`-DCORETRACE_COVERAGE=ON`. `scripts/ci/coverage.sh <build> <output>` then runs the tests and
writes a report for the unit tests, the integration tests and both, with an HTML report in
`<output>/html`. CI publishes the same report as the `coverage-report` artifact. On Linux, the
build links Clang's profile runtime (`libclang-rt-20-dev` from apt.llvm.org), and `LLVM_PROFDATA`
and `LLVM_COV` name the tools of your Clang (`/usr/lib/llvm-20/bin/llvm-profdata`, `llvm-cov`).

### CODE STYLE (clang-format)

- Version cible : `clang-format` 17 (utilisée dans la CI).
- Formater localement : `./scripts/format.sh`
- Vérifier sans modifier : `./scripts/format-check.sh`
- CMake : `cmake --build build --target format` ou `--target format-check`
- CI : le job GitHub Actions `clang-format` échoue si un fichier n’est pas formaté.

### DEBUG

You can pass arguments to CMake and invoke ASan.

```bash
-DUSE_THREAD_SANITIZER=ON
```
or
```bash
-DUSE_ADDRESS_SANITIZER=ON
```

> ⚠️ **Warning**: You cannot use `-DUSE_THREAD_SANITIZER=ON` and `-DUSE_ADDRESS_SANITIZER=ON` at the same time.

### ARGUMENT

The option list is generated from the binary and is the single source of truth:

```bash
./ctrace --help
```

### VERSION

```bash
./ctrace --version
```

Version resolution is centralized at configure time:

- release builds pass an explicit version string from CI, so published binaries print the exact GitHub release tag
- local Git builds resolve the version from `git describe --tags --dirty --always --match 'v*'`
- if Git metadata is unavailable, the build falls back to `dev`

You can also override the embedded version manually:

```bash
cmake .. -DCORETRACE_VERSION_OVERRIDE=v0.74.0
```

### EXIT CODES

`ctrace` is meant to gate a CI pipeline, so its exit code is a verdict on the run:

| Code | Meaning |
|---|---|
| `0` | Analysis complete, no finding at or above `--fail-on` |
| `1` | Usage or configuration error |
| `2` | Findings at or above `--fail-on` (default: `error`) |
| `3` | Analysis incomplete: a tool could not be started, crashed or exited abnormally |

`--fail-on error|warning|none` (config: `analysis.fail_on`) sets the threshold. `none` never
fails on findings but still returns `3` on a broken tool. Code `3` takes precedence over `2`.

```bash
./ctrace --fail-on warning --input src/main.c --invoke cppcheck,ctrace_stack_analyzer
```

### SARIF OUTPUT

`--sarif-format` reports the whole run as **one** SARIF 2.1.0 log, whatever tools ran: one
`run` per tool, rules collected per tool, a `coretrace/v1` partial fingerprint per result for
stable alerts across runs, and the CWE as a result property. The document is printed to
stdout and written to `--report-file`, ready for `github/codeql-action/upload-sarif`.

```bash
./ctrace --sarif-format --invoke cppcheck,flawfinder,ctrace_stack_analyzer \
    --input src/main.c --report-file ctrace.sarif
```

Every tool spells a file the same way: an input as it was typed on the command line, any
other file relative to the working directory when below it, else absolute. The locations and
the fingerprints of a run are therefore the same whichever tool reports the finding and on
whichever machine the run happens, when it runs from the repository root.

A weakness reported by several tools, same file, same line and same CWE, is one result: the
most severe report is kept in its tool's run, and its `alsoReportedBy` property names the other
tools and rules. Reports without a CWE cannot be matched and are all kept; the console output
and the counters still show every tool's report.

### CONFIGURATION

- Canonical default config: `config/tool-config.json`
- Full schema and semantics: `docs/configuration.md`
- Precedence: built-in defaults < config file < CLI

```bash
./ctrace --input ../tests/EmptyForStatement.cc --entry-points=main --verbose --static --dyn
```

### SERVER MODE

Start the HTTP server:

```bash
./ctrace --ipc serve --serve-host 127.0.0.1 --serve-port 8080 --shutdown-token mytoken
```

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
      "static_analysis": true,
      "dynamic_analysis": false,
      "invoke": ["flawfinder"],
      "sarif_format": true,
      "report_file": "ctrace-report.txt",
      "output_file": "ctrace.out",
      "ipc": "serve",
      "ipc_path": "/tmp/coretrace_ipc",
      "async": false,
      "verbose": true
    }
  }'
```

Response notes:
- `status` is `ok` or `error`.
- `result.diagnostics` lists every finding in one model (`tool`, `rule_id`, `file`, `line`, `column`, `severity`, `message`, `cwe`), whatever tool produced it; `result.diagnostics_summary_total` counts them by severity.
- `result.sarif` is the merged SARIF log when `sarif_format` is true (also written to `report_file`).
- `result.gate` carries the verdict the CLI would exit with: `fail_on`, `exit_code` (see EXIT CODES) and `failed_tools`.
- `result.uninterpreted_tools` names the tools whose output CoreTrace could not interpret: their findings are shown in `result.outputs` but are not counted.
- `result.outputs` groups tool output by tool name.
- Each output entry has `stream` and `message`. If a tool emits JSON, `message` is returned as a JSON object.

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
(up to `--shutdown-timeout-ms` if configured).

Each `run_analysis` request gets the same shipped defaults as the CLI: the stack analyzer
models and the bundled tools found next to the server's `ctrace` binary fill whatever the
request and its `config` file leave empty.

### Mangle/Demangle API

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
