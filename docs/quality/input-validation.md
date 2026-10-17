# Input validation and error handling

What `ctrace` checks at each of its inputs, how it reports what it rejects, and what it does when
something fails during a run.

| | |
|---|---|
| Date | 2026-10-15 |
| Commit | `eba7f88` |
| Method | A reading of the code, each behavior then checked by running `ctrace`: the scenario numbers (C01, S03...) refer to [Resilience scenarios](resilience.md) |

## The principle: fail closed

`ctrace` gates CI, so an input it cannot handle must never end in a silent pass. Every error ends
in a non-zero exit code, with a message on stderr:

| Exit code | Meaning | Examples |
|---:|---|---|
| 0 | Analysis complete, no finding at or above `--fail-on` | C11, C14, C30 |
| 1 | Usage or configuration error: nothing was analyzed | C01–C06, C15–C23 |
| 2 | Findings at or above `--fail-on` | C29 |
| 3 | Analysis incomplete: a tool could not start, crashed, timed out or failed | C10, C12, C13, C24–C26 |

A failed tool (3) wins over findings (2) and over `--fail-on none`
(`include/App/ExitPolicy.hpp:27-44`). The server returns the same verdict in `result.gate`.
`main.cpp:59-77` catches any exception that escapes, logs it and exits 1.

Two exceptions to fail-closed remain, tracked in [#189](https://github.com/CoreTrace/coretrace/issues/189): a run with no analysis selected
(C35) and tool output that cannot be read (C27) both exit 0.

## 1. Command line

All options are declared in one table (`src/App/Config.cpp:62-153`). Their values go through the
same loader as the configuration file, so the CLI and the file share the same rules
(`Config.cpp:359-371`).

| Input | Check | On error |
|---|---|---|
| Unknown option | CLI11 | `The following argument was not expected: --x`, exit 1 (C02) |
| Unsigned values: `--jobs`, `--serve-port`, `--shutdown-timeout-ms`, `--stack-limit`, `--smt-*` | Digits only, then range: `--jobs` 0–1024 (0 = one per core), port 0–65535 | `Invalid value for --jobs: 'abc' is not an unsigned integer.`, exit 1 (C04, C05) |
| Enumerations: `--fail-on`, `--ipc`, `--analysis-profile`... | Exact value from a fixed list | `Invalid value 'sometimes' for '--fail-on'. Allowed values: [none, warning, error]`, exit 1 (C03) |
| `--invoke` | Each name in the list of supported tools; duplicates removed | `--invoke: Unknown tool 'x'. Allowed tools: [...]`, exit 1 (C06) |
| `--serve-host`, `--serve-port` without `--ipc serve` | Rejected | exit 1 |
| Deprecated spellings (`--async`) | Accepted | A warning names the replacement |

## 2. Configuration file

`--config` and a server request's `config` are read by `src/App/ToolConfig.cpp`. Validation stops
at the first error, which is printed with the file name, and the run exits 1 before any tool
starts (`Config.cpp:344-355`, `main.cpp:47-53`).

| Rejected | Message | Scenario |
|---|---|---|
| File that cannot be opened | `Unable to open file: <path>` | C19 |
| Invalid JSON, or a root that is not an object | `Invalid JSON in file: <path>` | C15 |
| Unknown key, at any level | `Unknown key 'k' in 'analysis'. Allowed keys: [...]` | C16 |
| Wrong type | `Expected string for 'analysis.fail_on'.` | C17 |
| Value outside an enumeration | `Invalid value 'v' for 'analysis.fail_on'. Allowed values: [...]` | C18 |
| Out of range: port, jobs, timeouts, `cppcheck.jobs` | `server.port must be between 0 and 65535.` | — |
| Unsupported `schema_version` | `Unsupported schema_version '2'. Supported version: 1.` | — |
| Tool arguments that override what `ctrace` passes to the Python or runtime analyzer | `tools.X.args cannot override <arg>.` | — |

Legacy spellings are accepted with a warning (`Deprecated config key 'x' in 'y': use 'z'.`).
Relative paths of models and inputs are resolved from the configuration file's directory.

## 3. Input files and compilation databases

| Input | Behavior | Scenario |
|---|---|---|
| A path that does not exist | Passed to the tools, which fail: exit 3. Found late, without naming the file ([#190](https://github.com/CoreTrace/coretrace/issues/190)) | C07, C08 |
| A file that cannot be read | The tool fails: exit 3 | C10 |
| An empty file | Analyzed: nothing found, exit 0 | C11 |
| Binary content, or C that does not compile | The analyzer fails: exit 3 | C12, C13 |
| A path with spaces | Analyzed | C14 |
| A directory | Passed to the tools as is; cppcheck analyzes its content | C09 |
| A name that starts with `-` | Passed to the tools as `./-F.c`, so that none reads it as an option; findings are reported as `./-F.c`, as is any other file below the working directory whose relative path starts with `-` (a header in `-inc/`) | C37 |
| A file outside the working directory | Analyzed, and reported with its absolute path | — |
| `compile_commands.json` that is not JSON | `Invalid JSON in compile database '<path>'.`, exit 1 | C20 |
| Not an array of objects with a `file` string | `'<path>' is not a compile_commands.json document: expected an array of objects with a "file" field.`, exit 1 | C21 |
| Empty, or missing | `No input files resolved. Provide --input or --compile-commands.`, exit 1; a missing database is not named ([#190](https://github.com/CoreTrace/coretrace/issues/190)) | C22, C23 |

The extension decides the language: `.py` goes to the Python analyzer, anything else to the C and
C++ tools (`src/ctrace_tools/languageType.cpp`).

## 4. External tools and linked analyzers

| Failure | Detection | Result | Scenario |
|---|---|---|---|
| Tool not installed | Lookup in `PATH` before starting (`include/Process/UnixProcess.hpp:265-314`) | `Command not found in PATH: cppcheck`, exit 3 | C24 |
| Tool exits with an error | Exit status checked (`include/Process/Process.hpp:43-63`) | `cppcheck exited with exit code 1`, exit 3 | C07 |
| Tool killed by a signal | Same | `cppcheck was terminated by signal 11`, exit 3 | C25 |
| Tool that hangs | Deadline: `tools.<name>.timeout_s`, 600 s per file by default; the tool runs in its own process group, killed with SIGKILL at the deadline | `did not finish within its time limit and was stopped`, exit 3, no process left | C26 |
| Tool that throws inside `ctrace` | Caught per tool (`include/Process/Tools/ToolsInvoker.hpp:288-308`) | The other tools go on; exit 3 | — |
| Output that cannot be read | Lines that do not match are dropped; such a tool never changes the exit code ([#189](https://github.com/CoreTrace/coretrace/issues/189)) | exit 0 | C27 |
| Linked analyzers (stack, concurrency) | A compilation or analysis error marks them failed | exit 3 (C13); they take no timeout ([#172](https://github.com/CoreTrace/coretrace/issues/172)) | — |

Temporary files: each tool run gets its own directory (`mkdtemp`), removed when the run ends
(`include/Process/Tools/RunDirectory.hpp`). Tool output is captured in full, with no size limit.

## 5. Reports

A report that cannot be written is a warning, not a failure: the verdict is unchanged, because
the analysis itself completed (`src/App/Runner.cpp:84-93`).
`Unable to write the SARIF report: failed to create report directory '/no/such/dir'`, exit 2 as
the findings require (C28).

## 6. HTTP server

| Input | Check | Answer | Scenario |
|---|---|---|---|
| Bind address beyond loopback without `--shutdown-token` | Refused at startup (`src/App/Runner.cpp:27-56`) | exit 1 | S01 |
| Body larger than `server.max_body_bytes` (1 MiB) | httplib limit | 413 | S07 |
| Invalid JSON | Parse error caught | 400 `InvalidRequest` | S03 |
| Body, or a tool's output, that is not UTF-8 | The answer is written with the invalid bytes replaced by U+FFFD | 400 `InvalidRequest` for the body; the full result for a tool's output, in JSON and in SARIF | — |
| `proto` other than `coretrace-1.0` | Refused | 400 `UnsupportedProtocol` | S04 |
| Unknown method | Only `run_analysis` | 400 `UnknownMethod` | S05 |
| Parameters | The configuration loader's rules: unknown parameters, wrong types and values are rejected | 400 `InvalidParams` | S06 |
| Dynamic analysis without `--serve-allow-dynamic` | Refused; a request's own configuration cannot grant it | 403 `DynamicAnalysisDisabled` | S11 |
| Missing input | Resolution leaves no source file | 400 `MissingInput` | S12 |
| Other routes and methods | Not served | 404 | S08 |
| `POST /shutdown` without or with a wrong token | Refused | 403 | S09, S10 |
| Browser on another origin | No `Access-Control-*` header unless `server.cors_origin` is set | The page cannot read the answer | S13 |
| Concurrent requests | Worker pool; one tool pool shared by all requests | All answered | S16 |

An address and port already in use stop the server with exit 1 (S19). The guarantee is per
address and port:

- The host is resolved once, and only the first address the resolver gives is bound, with no
  fallback to the next one when it is taken. A second server on `localhost` therefore fails on
  the same address as the first, instead of starting on the other of `::1` and `127.0.0.1`.
  Which address comes first depends on the system's resolver configuration (on macOS,
  `localhost` gives `::1`): the policy removes the fallback, it does not promise the same choice
  to two resolutions made under different configurations, and clients must use the address
  the server logs, or the same name.
- `SO_REUSEADDR` is kept, so that a server restarts at once on a port its predecessor just
  closed. On macOS it also lets a server on `127.0.0.1` start while another listens on
  `0.0.0.0` with the same port: the two addresses differ.

A failure on the server's side is answered with 500 `InternalError`, its details only in the
log: an unexpected exception, or a resource of the server's side that is not available, such as
an IPC socket (`ipc: socket`, deprecated) that nobody listens on. Three
findings about what a request can make the server do are reported privately, as
[SECURITY.md](../../SECURITY.md) asks.

## 7. Signals

SIGINT and SIGTERM are recorded by a handler (`include/Process/Interrupt.hpp`), and acted on
where ctrace waits ([#188](https://github.com/CoreTrace/coretrace/issues/188)):

- A run stops the external tools it runs: SIGTERM to each tool's process group, which holds
  what the tool started, then SIGKILL after 2 s (`include/Process/UnixProcess.hpp`). A tool's
  group never outlives it: what the tool left there is killed when it ends, so that an interrupt
  handled just after, on any thread, leaves nothing behind. No tool starts after the interrupt, a linked analyzer included
  (`ToolInvoker::runTool`), and the run directories are removed as the tool runs return. The
  run gives no report or verdict, and `ctrace` ends by the signal it received: 130 or 143 in a
  shell (C31–C34). A linked analyzer (stack, concurrency) already running cannot be stopped
  ([#172](https://github.com/CoreTrace/coretrace/issues/172)): `ctrace` waits for it.
- A server drains as `POST /shutdown` does (S17): it stops accepting connections, answers the
  requests in flight, their tools left to finish, then ends by the signal (S18, S21).
- A second interrupt ends `ctrace` at once, whatever is still running.
- A signal ignored when `ctrace` started (a background job of a script) stays ignored.

## Tests

Most of these behaviors have a test, named after it in `CMakeLists.txt`: for example
`ctrace_exit_1_without_input`, `ctrace_exit_3_when_a_tool_cannot_run`,
`ctrace_unwritable_sarif_report_keeps_the_verdict`, `ctrace_dyn_hung_tool_is_stopped`,
`ctrace_serve_refuses_an_exposed_host_without_token`, and the unit tests `ctrace_config_tests`,
`ctrace_files_tests`, `ctrace_process_tests`, `ctrace_server_http_tests`. Signals are tested by
`ctrace_interrupt_tests`.

Not tested yet: invalid JSON in a configuration file, the HTTP 404 and 503 answers, a
malformed `arguments` field in a compilation database.
