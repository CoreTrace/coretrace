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
Impact: runs static tool set when true.
CLI: `--static`

- `analysis.dynamic`
Type: `bool`
Default: `false`
Allowed: `true|false`
Description: enable dynamic analysis pipeline.
Impact: runs dynamic tool set when true.
CLI: `--dyn`

- `analysis.invoke`
Type: `string|string[]`
Default: `[]`
Allowed: `flawfinder|ikos|cppcheck|tscancode|ctrace_stack_analyzer`
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
Allowed: writable path.
Description: generic output artifact path.
Impact: unified output target for integrations that use this field.
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
Impact: enables thread-pool based tool scheduling.
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

### Exposure

The API has no authentication. Binding `server.host` to anything outside the loopback
interface therefore requires `server.shutdown_token`; the server refuses to start otherwise.
The token protects `POST /shutdown` only, so treat a non-loopback bind as giving anyone who
can reach the port the ability to run analyses on this machine.

## tools

External tools are invoked by name and resolved through `PATH`. Give a tool an explicit
location only when it is not on `PATH` or when you need a specific build.

- `tools.<name>.path`
Type: `string`
Default: the tool's own name (`cppcheck`, `ikos`, `tscancode`, `flawfinder`)
Allowed: a command name resolved through `PATH`, or an absolute path.
Description: the command to execute for that tool.
Impact: replaces the default lookup. A command that cannot be found or executed is reported
with its name and the tool is skipped.
CLI: not exposed (`config/tool-config.json` only)

- `tools.<name>.args`
Type: `string|string[]`
Default: `[]`
Allowed: any arguments the tool accepts.
Description: extra command-line arguments for that tool.
Impact: appended verbatim after the options CoreTrace derives and before the input file, so
they can override a derived option (for cppcheck, `--disable=style` turns the style checks
back off, `--suppress=<id>` silences a rule). This is the escape hatch for what the schema
does not model; project includes and defines belong in `stack_analyzer.include_dirs` and
`stack_analyzer.defines`, which every tool that understands them receives.
CLI: not exposed (`config/tool-config.json` only)

```json
{
  "tools": {
    "cppcheck": {"path": "/opt/homebrew/bin/cppcheck", "args": ["--std=c++20", "--suppress=unusedFunction"]},
    "flawfinder": {"path": "flawfinder-3"}
  }
}
```

Derived cppcheck options: `--enable=warning,style,performance,portability` and
`--inline-suppr` always; `-I<dir>` for each `stack_analyzer.include_dirs` entry, `-D<macro>`
for each `stack_analyzer.defines` entry, and `-j N` when `stack_analyzer.jobs` is a number.

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
