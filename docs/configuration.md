# Configuration Reference

`config/tool-config.json` is the canonical configuration file.

## Precedence Rules

1. Built-in defaults (`ProgramConfig` in `include/Config/config.hpp`, one struct per section below)
2. Configuration file (`--config`)
3. CLI options (last override)

CLI values always override config file values.

## Schema Version

- `schema_version`
Type: `uint`
Default: `1`
Allowed: `1`
Description: schema compatibility gate.
Impact: rejects unsupported schemas with explicit diagnostics.

## analysis

- `analysis.static`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: enable static analysis pipeline.
Impact: runs the static tool set when true: `cppcheck`, `flawfinder`, `tscancode`, `ikos`,
`ctrace_stack_analyzer`, `coretrace-concurrency-analyzer` and `coretrace-python-analyzer`, each
on the files of its language.
CLI: `--static`

- `analysis.dynamic`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: enable dynamic analysis.
Impact: runs `coretrace-runtime-analyzer` on each C/C++ input, which must be a whole program
(it defines `main`): the tool builds it with CoreTrace instrumentation, runs it, and reports
the memory errors that happened (heap and stack overflows, uses after free, double frees,
leaks, vtable misuse). It runs the program with `ctrace`'s privileges: a separate process, not
a sandbox. Linking the program needs a C++ build environment (`g++` on Debian and Ubuntu,
`gcc-c++` on RHEL). See `tools.coretrace-runtime-analyzer.timeout_s`.
CLI: `--dyn`

- `analysis.invoke`
Type: `string|string[]`
Default: `[]`
Allowed: `flawfinder|ikos|cppcheck|tscancode|ctrace_stack_analyzer|coretrace-concurrency-analyzer|coretrace-python-analyzer|coretrace-runtime-analyzer`
Description: explicit tool selection.
Impact: runs only selected tools through specific-tool path.
CLI: `--invoke`

- `analysis.fail_on`
Type: `string`
Default: `"error"`
Allowed: `error|warning|none`
Description: lowest severity that makes the run exit non-zero.
Impact: the process exits `2` when a finding at or above this level was reported, `0`
otherwise. `none` never fails on findings. Independently of this value, the process exits `3`
when a tool could not be started, crashed or exited abnormally, and `1` on a usage or
configuration error. Server mode returns the same verdict in `result.gate`.
CLI: `--fail-on`

## files

- `files.input`
Type: `string|string[]`
Default: `[]`
Allowed: source file paths, and/or `compile_commands.json` documents (Clang schema: an array
of objects with a `file` field and an optional `directory`).
Description: input source set.
Impact: resolved and analyzed; relative paths in the config file are resolved from its
directory, and entries of a compile database are resolved from their own `directory`. Any
other `.json` input is rejected with an explicit error.
CLI: `--input`

- `files.entry_points`
Type: `string|string[]`
Default: `["main"]`
Allowed: function names.
Description: entry-point filter list.
Impact: forwarded to tools supporting entry-point filtering.
CLI: `--entry-points`

- `files.compile_commands`
Type: `string`
Default: `""`
Allowed: path to `compile_commands.json` or directory containing it.
Description: compilation database for analyzer context.
Impact: enables compile database driven file resolution and analyzer context.
CLI: `--compile-commands`

- `files.include_compdb_deps`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: include dependency entries (`_deps`) from auto-discovered compdb inputs.
Impact: broadens or narrows analyzed source set.
CLI: `--include-compdb-deps`

## output

- `output.sarif_format`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: report the whole run as one SARIF 2.1.0 log.
Impact: every tool's findings are normalized and rendered into a single document with one
`run` per tool, printed to stdout and written to `output.report_file`. Tools print nothing
else to stdout in this mode; the stack analyzer's own report is not written (its findings
are in the merged document), and a tool whose output is not interpreted (ikos) is reported
with a warning and kept in `result.outputs` in server mode. Without this flag each finding
is printed as `file:line:col: severity: message [tool/rule]`.
CLI: `--sarif-format`

- `output.report_file`
Type: `string`
Default: `"ctrace-report.txt"`
Allowed: writable path.
Description: report output path.
Impact: IKOS receives this path via `--report-file`; stack analyzer stdout report is persisted to this file by coretrace after a successful run.
CLI: `--report-file`

- `output.output_file`
Type: `string`
Default: `"ctrace.out"`
Allowed: path string.
Description: legacy output artifact path.
Impact: retained for schema compatibility; no current tool bridge consumes this field, so it
does not create an artifact. Use `output.report_file` for the analysis report. The runtime
analyzer uses a private temporary binary that is removed after each run.
CLI: `--output-file`

- `output.verbose`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: verbose logs.
Impact: enables debug-level logs and bridge decision traces.
CLI: `--verbose`

- `output.quiet`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: reduced output mode.
Impact: forwarded to stack analyzer and used by core logging behavior.
CLI: `--quiet`

- `output.demangle`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: demangle symbol names when supported.
Impact: forwarded to stack analyzer and other supported tools.
CLI: `--demangle`

## runtime

- `runtime.async`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: async execution policy.
Impact: runs the tool runs on a thread pool with one worker per core: each file is one run of
each per-file tool, so different files are analyzed at the same time, and a slow file holds
one worker, not the others. `coretrace-runtime-analyzer` runs one program at a time. Findings
are reported in the same order as in a sequential run (by tool, file, line and column).
CLI: `--async`

- `runtime.ipc`
Type: `string`
Default: `"standardIO"`
Allowed: `standardIO|socket|serve`
Description: IPC mode.
Impact: selects standard output, socket transport, or HTTP server mode.
Deprecation: `socket` is deprecated and will be removed in a future release. Only some tools
ever wrote to the socket, and nothing in the project reads from it. It still works and emits a
warning; use `standardIO`, or `serve` to consume results over HTTP.
CLI: `--ipc`

- `runtime.ipc_path`
Type: `string`
Default: `"/tmp/coretrace_ipc"`
Allowed: socket path.
Description: IPC socket path.
Impact: used when IPC mode is socket, which is deprecated (see `runtime.ipc`).
CLI: `--ipc-path`

## server

- `server.host`
Type: `string`
Default: `"127.0.0.1"`
Allowed: valid host/ip.
Description: HTTP server bind host.
Impact: controls server listen interface in `serve` mode.
CLI: `--serve-host`

- `server.port`
Type: `uint`
Default: `8080`
Allowed: `0..65535`
Description: HTTP server bind port.
Impact: controls server listen port in `serve` mode.
CLI: `--serve-port`

- `server.shutdown_token`
Type: `string`
Default: `""`
Allowed: any non-empty token for shutdown auth.
Description: shutdown endpoint auth token.
Impact: required for authenticated shutdown requests.
CLI: `--shutdown-token`

- `server.shutdown_timeout_ms`
Type: `uint`
Default: `0`
Allowed: `0..INT_MAX`
Description: graceful shutdown timeout.
Impact: waits for in-flight requests up to timeout (`0` = wait indefinitely).
CLI: `--shutdown-timeout-ms`

- `server.max_body_bytes`
Type: `uint`
Default: `1048576` (1 MiB)
Allowed: `0..UINT64_MAX`, `0` meaning no limit.
Description: largest accepted request body.
Impact: a larger body is answered with `413`. Requests carry a file list, not file contents.
CLI: not exposed (`config/tool-config.json` only)

- `server.cors_origin`
Type: `string`
Default: `""`
Allowed: an origin such as `https://gui.example`.
Description: origin allowed to call the API from a browser.
Impact: empty sends no `Access-Control-*` header, so a page on another origin cannot read the
response. Set it only for the origin of your own front end.
CLI: not exposed (`config/tool-config.json` only)

- `server.allow_dynamic_analysis`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: whether requests may run dynamic analysis (`dynamic_analysis`, or
`coretrace-runtime-analyzer` in `invoke`).
Impact: dynamic analysis builds and runs the submitted code on this machine, with the server's
privileges. Without this setting, such a request is refused (`DynamicAnalysisDisabled`). A
request's own `config` file cannot set it: only the server's configuration can. Run the server
in an isolated environment (container, dedicated user) before enabling it.
CLI: `--serve-allow-dynamic`

### Exposure

The API has no authentication. Binding `server.host` to anything outside the loopback
interface therefore requires `server.shutdown_token`; the server refuses to start otherwise.
The token protects `POST /shutdown` only, so treat a non-loopback bind as giving anyone who
can reach the port the ability to run analyses on this machine, and, with
`server.allow_dynamic_analysis`, to run code on it.

## tools

External tools are looked up in this order: `tools.<name>.path` when set; then the copy
shipped with `ctrace`, an executable at `<prefix>/libexec/coretrace/<name>/<name>` next to
`bin/ctrace` (or `<build>/libexec/coretrace/<name>/<name>` in a build tree), or
`<name>/bin/runtime-analyzer` for `coretrace-runtime-analyzer`, which ships as its own release
tree; then the tool's name resolved through `PATH`. Give a tool an explicit location only when
you need a specific build.

Each input file only reaches the tools of its language: `.py` files go to
`coretrace-python-analyzer`, every other file to the C/C++ tools. `coretrace-python-analyzer`
runs once per Python project (the nearest directory above an input that holds
`pyproject.toml`, `setup.py`, `setup.cfg` or `.git`, else the input's directory) and analyzes
all of it, so flows across modules are found; only findings located in an input, or about the
project itself (a vulnerable pin in a dependency manifest), are reported.

- `tools.<name>.path`
Type: `string`
Default: the bundled copy when there is one, else the tool's own name (`cppcheck`, `ikos`,
`tscancode`, `flawfinder`, `coretrace-python-analyzer`, `coretrace-runtime-analyzer`)
Allowed: a command name resolved through `PATH`, or an absolute path.
Description: the command to execute for that tool.
Impact: replaces the default lookup. A command that cannot be found or executed is reported
with its name and the tool is skipped.
CLI: not exposed (`config/tool-config.json` only)

- `tools.<name>.args`
Type: `string|string[]`
Default: `[]`
Allowed: arguments for the selected tool that preserve the output CoreTrace parses.
Description: extra command-line arguments for that tool.
Impact: appended verbatim after the options CoreTrace derives and before the input file, so
they can override a derived option (for cppcheck, `--disable=style` turns the style checks
back off, `--suppress=<id>` silences a rule). The Python and runtime bridges reject options
that change their required SARIF report or execution mode. This covers `--format`,
`--emit-ir` for Python, and `--no-run` or `--show-output` for runtime. Other tool options
remain available through this escape hatch. Project includes and defines belong in
`stack_analyzer.include_dirs` and `stack_analyzer.defines`, which every tool that understands
them receives.
CLI: not exposed (`config/tool-config.json` only)

```json
{
  "tools": {
    "cppcheck": {"path": "/opt/homebrew/bin/cppcheck", "args": ["--std=c++20", "--suppress=unusedFunction"]},
    "flawfinder": {"path": "flawfinder-3"}
  }
}
```

- `tools.<name>.timeout_s`
Type: `uint`
Default: `600`
Allowed: seconds; `0` disables the limit.
Description: how long one run of an external tool (`cppcheck`, `flawfinder`, `tscancode`,
`ikos`, `coretrace-python-analyzer`) may take.
Impact: a run still going after this time is stopped and reported as failed, which makes the
analysis incomplete (exit code 3); the other runs go on. `coretrace-concurrency-analyzer` and
the stack analyzer are linked into `ctrace` and cannot be stopped, so they take no timeout.
`coretrace-runtime-analyzer` gives the key its own meaning, below.
CLI: not exposed (`config/tool-config.json` only)

```json
{
  "tools": {
    "ikos": {"timeout_s": 1800},
    "cppcheck": {"timeout_s": 0}
  }
}
```

- `tools.coretrace-runtime-analyzer.timeout_s`
Type: `uint`
Default: `60`
Allowed: seconds; `0` disables the limit.
Description: how long each program may run under the runtime analyzer.
Impact: passed to the tool as `--timeout`; a program that runs longer is stopped and reported
as an incomplete analysis. `ctrace` also stops the tool itself after twice this time, which
covers building the program, and reports it the same way.
CLI: not exposed (`config/tool-config.json` only)

The runtime analyzer receives `--format sarif --timeout <timeout_s> -o <run directory>/program`,
then `tools.coretrace-runtime-analyzer.args`, then `--` followed by `-I`/`-D` from
`stack_analyzer.include_dirs` and `stack_analyzer.defines`,
`tools.coretrace-runtime-analyzer.compile_args`, and the input. Each program is built
in a private temporary directory, removed afterwards. Exit code 2 from the tool (the program
could not be built or run) makes the analysis incomplete.

- `tools.coretrace-runtime-analyzer.compile_args`
Type: `string|string[]`
Default: `[]`
Description: extra C/C++ compiler arguments after the runtime analyzer's `--` separator.
Impact: supports flags such as `-std=c++20` and `-Wall`; these are compiler arguments, not
runtime analyzer options. The binary output remains in the private run directory.

- `tools.cppcheck.jobs`
Type: positive integer
Default: unset (cppcheck default)
Description: cppcheck worker count, forwarded as `-j N`.
Impact: when unset, a numeric `stack_analyzer.jobs` still supplies `-j N` for compatibility
with existing schema v1 configurations. The cppcheck setting takes precedence.

- `tools.coretrace-concurrency-analyzer.rules`
Type: `string|string[]`
Default: `[]` (every rule)
Allowed: `data-race|missing-join|deadlock-lock-order|condition-wait|fork-after-thread|unreaped-child|thread-arg-escape|unsafe-signal-handler|weak-publication|thread-arg-freed|thread-local-escape`
Description: the rules the concurrency analyzer runs, by the names of its own `--rules` option.
Impact: only the selected rules report. An unknown name is reported when the tool runs, and the
analysis is incomplete (exit code 3).
CLI: not exposed (`config/tool-config.json` only)

- `tools.coretrace-concurrency-analyzer.max_live_units`
Type: positive integer
Default: unset (analyzer default)
Description: upper bound on concurrently loaded project units in the in-process analyzer.
Impact: applies to project analysis with a compilation database; it does not change the
number of worker threads. The bridge already uses no disk cache, so the analyzer's CLI
`--no-cache` mode needs no separate configuration key.

[coretrace-concurrency-analyzer](https://github.com/CoreTrace/coretrace-concurrency-analyzer) is
linked into `ctrace`, like the stack analyzer: it has no `path` and no `args`, and needs no clang
installed. It reports data races on shared globals (`DataRaceGlobal`, CWE-362), lock-order
deadlocks (`DeadlockLockOrder`, CWE-833), missing joins, condition waits without a predicate,
thread arguments escaping their frame or freed early, unsafe signal handlers and weak publication
ordering. With `files.compile_commands`, or a `compile_commands.json` among the inputs, the C/C++
inputs are analyzed as one program: each unit is built with the arguments the database records
for it (without its output, dependency-file and optimization options), so a thread started in
one unit is related to its body in another, and a unit the database does not list makes the
analysis incomplete. Without a database, each input is analyzed on its own, built with `-I`/`-D`
from `stack_analyzer.include_dirs` and `stack_analyzer.defines`. A finding carries its rule, its
CWE, the analyzer's confidence as a property, and the other locations involved (the conflicting
access of a race, the other lock of a cycle) as `note:` lines after it and as SARIF related
locations; a finding the analyzer rates with low confidence is a warning at most.

Derived cppcheck options: `--enable=warning,style,performance,portability` and
`--inline-suppr` always; `-I<dir>` for each `stack_analyzer.include_dirs` entry, `-D<macro>`
for each `stack_analyzer.defines` entry, and `-j N` from `tools.cppcheck.jobs` or the legacy
numeric `stack_analyzer.jobs` fallback.

`tools.ctrace_stack_analyzer` and `tools.stack_analyzer` keep their legacy meaning: they are
alternative spellings of the `stack_analyzer` section below. The stack analyzer runs in
process and has no command to resolve.

## stack_analyzer

- `stack_analyzer.mode`
Type: `string`
Default: `"ir"`
Allowed: analyzer-supported modes.
Description: stack analyzer execution mode.
Impact: forwarded as `--mode=<value>`.
CLI: not exposed (`config/tool-config.json` only)

- `stack_analyzer.output_format`
Type: `string`
Default: `""`
Allowed: analyzer-supported formats (`json`, `sarif`, `text`, ...).
Description: explicit stack analyzer output format for its own report.
Impact: forwarded as `--format=<value>` when non-empty; ignored under `--sarif-format`, where
the analyzer renders nothing and its findings go to the merged document.
CLI: not exposed (`config/tool-config.json` only)

- `stack_analyzer.timing`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: enable timing stats from stack analyzer.
Impact: forwarded as `--timing`.
CLI: `--timing`

- `stack_analyzer.analysis_profile`
Type: `string`
Default: `""`
Allowed: `fast|full|""`
Description: profile selection.
Impact: forwarded as `--analysis-profile` when non-empty.
CLI: `--analysis-profile`

- `stack_analyzer.smt`
Type: `bool|string`
Default: `""`
Allowed: bool-like values (`true/false`, `on/off`, `1/0`, `yes/no`).
Description: SMT enable switch.
Impact: normalized and forwarded as `--smt on|off`.
CLI: `--smt`

- `stack_analyzer.smt_backend`
Type: `string`
Default: `""`
Allowed: backend names supported by analyzer.
Description: primary SMT backend.
Impact: forwarded as `--smt-backend` when non-empty.
CLI: `--smt-backend`

- `stack_analyzer.smt_secondary_backend`
Type: `string`
Default: `""`
Allowed: backend names supported by analyzer.
Description: secondary SMT backend.
Impact: forwarded as `--smt-secondary-backend` when non-empty.
CLI: `--smt-secondary-backend`

- `stack_analyzer.smt_mode`
Type: `string`
Default: `""`
Allowed: `single|portfolio|cross-check|dual-consensus|""`
Description: SMT orchestration mode.
Impact: forwarded as `--smt-mode` when non-empty.
CLI: `--smt-mode`

- `stack_analyzer.smt_timeout_ms`
Type: `uint`
Default: `0`
Allowed: `0..UINT32_MAX`
Description: SMT timeout per query.
Impact: forwarded as `--smt-timeout-ms` when > 0.
CLI: `--smt-timeout-ms`

- `stack_analyzer.smt_budget_nodes`
Type: `uint`
Default: `0`
Allowed: `0..UINT64_MAX`
Description: SMT budget cap.
Impact: forwarded as `--smt-budget-nodes` when > 0.
CLI: `--smt-budget-nodes`

- `stack_analyzer.smt_rules`
Type: `string|string[]`
Default: `[]`
Allowed: analyzer rule ids.
Description: SMT-enabled rule subset.
Impact: forwarded as `--smt-rules` CSV when non-empty.
CLI: `--smt-rules`

- `stack_analyzer.stack_limit`
Type: `uint`
Default: `8388608`
Allowed: `0..UINT64_MAX`
Description: stack bound limit in bytes.
Impact: forwarded as `--stack-limit` when > 0.
CLI: `--stack-limit`

- `stack_analyzer.assume_external_frame`
Type: `string|uint`
Default: unset (unresolved calls keep the stack bound unknown)
Allowed: non-negative bytes, optionally with a KiB/MiB/GiB suffix accepted by the analyzer.
Description: estimated frame size charged for each unresolved external call.
Impact: forwarded as `--assume-external-frame` when set. `0` ignores those frames.
CLI: not exposed (`config/tool-config.json` only)

- `stack_analyzer.resource_model`
Type: `string`
Default: `<models dir>/resource-lifetime/generic.txt` when that file exists (see below), else `""`
Allowed: file path.
Description: resource lifetime model path.
Impact: forwarded as `--resource-model` when non-empty; relative paths resolved from config dir.
CLI: `--resource-model`

- `stack_analyzer.escape_model`
Type: `string`
Default: `<models dir>/stack-escape/generic.txt` when that file exists (see below), else `""`
Allowed: file path.
Description: escape model path.
Impact: forwarded as `--escape-model` when non-empty; relative paths resolved from config dir.
CLI: `--escape-model`

- `stack_analyzer.buffer_model`
Type: `string`
Default: `<models dir>/buffer-overflow/generic.txt` when that file exists (see below), else `""`
Allowed: file path.
Description: buffer model path.
Impact: forwarded as `--buffer-model` when non-empty; relative paths resolved from config dir.
CLI: `--buffer-model`

Default models: when a model key is still empty after the config file and the command line,
`ctrace` looks for the models shipped next to the executable, in the first existing of
`<exe dir>/../config/models` (install prefix, or a `build/` directory inside the repository)
and `<exe dir>/config/models` (any build directory). When the stack analyzer is selected and a
default model cannot be found, a startup warning names the key so the loss of its rule family
is visible. An explicit value is never overridden.

- `stack_analyzer.extra_args`
Type: `string|string[]`
Default: `[]`
Allowed: analyzer CLI tokens.
Description: generic extension point for analyzer runtime flags not explicitly modeled.
Impact: forwarded verbatim after mapped options.
CLI: not exposed (`config/tool-config.json` only)

## Validation Behavior

- Unknown root keys are rejected with allowed-key diagnostics.
- Unknown section keys are rejected with allowed-key diagnostics.
- Type errors are rejected with path-qualified diagnostics.
- Enum-like values (`ipc`, `analysis_profile`, `smt_mode`) are validated explicitly.

## Legacy Compatibility

The loader still accepts legacy shapes:

- root `invoke`
- root `input` as string or array
- `stack_analyzer` legacy keys (`analysis-profile`, `smt-*`, `entry_points`, etc.)
- `tools.ctrace_stack_analyzer` / `tools.stack_analyzer`

When both legacy and canonical sections are present, canonical sections (`analysis`, `files`, `output`, `runtime`, `server`) are applied last and therefore take precedence inside the config file.

Every legacy spelling is reported when the file is loaded, on stderr for the CLI and in the
server log for a request that names a config file, with the canonical key to use instead:

```
Warning: Deprecated config key 'analysis-profile' in 'stack_analyzer': use 'analysis_profile'.
```

Legacy spellings will be removed in a future release; a canonical document produces no
warning.
