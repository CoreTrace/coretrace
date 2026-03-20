# Configuration Reference

`config/tool-config.json` is the canonical configuration file.

## Precedence Rules

1. Built-in defaults (`GlobalConfig`)
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

## files

- `files.input`
Type: `string|string[]`
Default: `[]`
Allowed: source files and/or JSON manifests.
Description: input source set.
Impact: resolved and analyzed; relative paths are resolved from config file directory.
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
Description: enable SARIF-oriented output behavior.
Impact: toggles SARIF-related output mapping/forwarding.
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
CLI: `--ipc`

- `runtime.ipc_path`
Type: `string`
Default: `"/tmp/coretrace_ipc"`
Allowed: socket path.
Description: IPC socket path.
Impact: used when IPC mode is socket.
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
Description: explicit stack analyzer output format.
Impact: forwarded as `--format=<value>`; if empty and SARIF is enabled, bridge uses `--format=json`.
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
Default: `""`
Allowed: file path.
Description: resource lifetime model path.
Impact: forwarded as `--resource-model` when non-empty; relative paths resolved from config dir.
CLI: `--resource-model`

- `stack_analyzer.escape_model`
Type: `string`
Default: `""`
Allowed: file path.
Description: escape model path.
Impact: forwarded as `--escape-model` when non-empty; relative paths resolved from config dir.
CLI: `--escape-model`

- `stack_analyzer.buffer_model`
Type: `string`
Default: `""`
Allowed: file path.
Description: buffer model path.
Impact: forwarded as `--buffer-model` when non-empty; relative paths resolved from config dir.
CLI: `--buffer-model`

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
