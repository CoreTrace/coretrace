# Resilience scenarios

How `ctrace` behaves when its inputs are wrong, when the tools it runs fail, and when it is
interrupted: 58 scenarios run against the real binary, with their results.

| | |
|---|---|
| Date | 2026-10-15 |
| Commit | `eba7f88` (`ctrace v0.84.0-82-geba7f88`), built as the README describes |
| Machine | macOS 15.6.1, Apple M3; cppcheck 2.17.1, flawfinder 2.0.20. S19 also on Ubuntu 24.04 (arm64, release v0.84.0) |
| Expected behavior | [Input validation and error handling](input-validation.md) |

## Summary

| | Command line | Server | Total |
|---|---:|---:|---:|
| Scenarios | 37 | 21 | 58 |
| As expected | 25 | 14 | 39 |
| Safe but late or unclear | 4 | 3 | 7 |
| Wrong behavior | 8 | 1 | 9 |
| Security findings, reported privately | 0 | 3 | 3 |

Every failure of a tool, every invalid configuration and every invalid request ends in a non-zero
exit code or an error answer, never in a silent pass, with two exceptions (C27, C35). The wrong
behaviors are tracked in four issues; the security findings follow [SECURITY.md](../../SECURITY.md).

## Fixtures

- `ok.c`: a loop that writes one element past a 4-element array (one finding, a warning).
- `broken.c` (does not compile), `empty.c`, `garbage.c` (4 KiB of random bytes), `noread.c` (mode
  000), `dir with space/ok.c`.
- Configuration files: malformed JSON, an unknown key, a wrong type, a wrong enumeration value,
  `tools.cppcheck.timeout_s: 2`.
- Compilation databases: not JSON, wrong field types, an empty array.
- Fake `cppcheck` executables put first in `PATH`: one kills itself with SIGSEGV, one sleeps 600
  s, one prints 2 KB of random bytes and exits 0.

Each scenario runs `ctrace` with a 120-second guard and records its exit code, its duration, the
tool processes left behind, and its last error line.

## Command line

| # | Scenario | Command (`ctrace ...`) | Result | Verdict |
|---|---|---|---|---|
| C01 | No input | `--invoke cppcheck` | 1, `No input files resolved` | As expected |
| C02 | Unknown option | `--input ok.c --no-such-option` | 1, `The following argument was not expected` | As expected |
| C03 | Invalid `--fail-on` | `--fail-on sometimes` | 1, lists the allowed values | As expected |
| C04 | `--jobs 0` | `--jobs 0` | Runs, one job per core | As expected |
| C05 | Non-numeric `--jobs` | `--jobs abc` | 1, `is not an unsigned integer` | As expected |
| C06 | Unknown tool | `--invoke no_such_tool` | 1, lists the supported tools | As expected |
| C07 | Missing input, external tool | `--input missing.c --invoke cppcheck` | 3, `cppcheck exited with exit code 1` | Safe but late |
| C08 | Missing input, linked analyzer | `--input missing.c --invoke ctrace_stack_analyzer` | 3 | Safe but late |
| C09 | A directory as input | `--input <dir> --invoke cppcheck` | cppcheck analyzes the directory, 2 | Accepted as is |
| C10 | Unreadable file | `--input noread.c` | 3 | As expected |
| C11 | Empty file | `--input empty.c` | 0 | As expected |
| C12 | Random bytes | `--input garbage.c` | 3 | As expected |
| C13 | Does not compile | `--input broken.c` | 3 | As expected |
| C14 | Path with spaces | `--input "dir with space/ok.c"` | Analyzed, 0 | As expected |
| C15 | Malformed configuration | `--config cfg-malformed.json` | 1, `Invalid JSON in file` | As expected |
| C16 | Unknown configuration key | `--config cfg-unknown-key.json` | 1, lists the allowed keys | As expected |
| C17 | Wrong type | `--config cfg-bad-type.json` | 1, `Expected string for 'analysis.fail_on'` | As expected |
| C18 | Wrong enumeration value | `--config cfg-bad-enum.json` | 1, lists the allowed values | As expected |
| C19 | Missing configuration | `--config no-such-config.json` | 1, `Unable to open file` | As expected |
| C20 | Database that is not JSON | `--compile-commands cc-notjson.json` | 1, `Invalid JSON in compile database` | As expected |
| C21 | Database with wrong types | `--compile-commands cc-badtypes.json` | 1, `is not a compile_commands.json document` | As expected |
| C22 | Empty database | `--compile-commands cc-empty.json` | 1, `No input files resolved` | As expected |
| C23 | Missing database | `--compile-commands no-such-db.json` | 1, `No input files resolved`, without naming the file | Safe but unclear |
| C24 | Tool not installed | `--invoke cppcheck`, `PATH` without it | 3, `Command not found in PATH: cppcheck` | As expected |
| C25 | Tool crashes | fake cppcheck, SIGSEGV | 3, `terminated by signal 11` | As expected |
| C26 | Tool hangs | fake cppcheck, `timeout_s: 2` | 3 after 2.1 s, `was stopped`, no process left | As expected |
| C27 | Tool prints unreadable output | fake cppcheck, random bytes, exit 0 | **0**, no warning | Wrong: passes the gate |
| C28 | Report cannot be written | `--sarif-format --report-file /no/such/dir/r.sarif --fail-on warning` | 2, warning `Unable to write the SARIF report` | As expected |
| C29 | Finding at the threshold | `--fail-on warning` | 2 | As expected |
| C30 | Threshold `none` | `--fail-on none` | 0 | As expected |
| C31 | Ctrl+C during a tool run | SIGINT to the process group | 130, **tool still running, run directory left** | Wrong |
| C32 | SIGTERM during a tool run | SIGTERM to the process group | 143, **tool still running, run directory left** | Wrong |
| C33 | SIGINT to `ctrace` only | SIGINT to its pid | 130, same | Wrong |
| C34 | SIGTERM to `ctrace` only | SIGTERM to its pid | 143, same | Wrong |
| C35 | No analysis selected | `--input ok.c` | **0** | Wrong: passes the gate |
| C36 | Overlapping selections | `--static --invoke cppcheck` | cppcheck runs twice, its findings counted twice | Wrong |
| C37 | File name starting with `-` | `--input ./-F.c --invoke flawfinder` | 3, flawfinder reads it as an option | Wrong |

## Server

The server runs with `--ipc serve --serve-host 127.0.0.1 --shutdown-token <token>`; requests are
sent with `curl` to `POST /api`.

| # | Scenario | Result | Verdict |
|---|---|---|---|
| S01 | Bind `0.0.0.0` without a token | Refused, exit 1 | As expected |
| S02 | Valid request | 200, `status: ok`, `gate.exit_code` 0 | As expected |
| S03 | Malformed JSON | 400 `InvalidRequest` | As expected |
| S04 | `proto: coretrace-9.9` | 200, the request is processed (here `NoAnalysisSelected`): the protocol is not checked | Unclear: protocol not checked |
| S05 | Unknown method | 200 `UnknownMethod` | Unclear: error answered with 200 |
| S06 | Wrong parameter type | 200 `InvalidParams`, `Expected unsigned integer for 'jobs'` | As expected |
| S07 | 2 MB body | 413 | As expected |
| S08 | `GET /api` | 404 | As expected |
| S09 | `POST /shutdown` without a token | 403 | As expected |
| S10 | `POST /shutdown` with a wrong token | 403 | As expected |
| S11 | Dynamic analysis without the opt-in | 200 `DynamicAnalysisDisabled` | As expected |
| S12 | Input that does not exist | 200, `gate.exit_code` 3 | As expected |
| S13 | Request from another origin | No `Access-Control-Allow-Origin` header | As expected |
| S14 | Security finding | Reported privately | Finding |
| S15 | Security finding | Reported privately | Finding |
| S16 | 8 concurrent requests | 8 × 200, server alive | As expected |
| S17 | `POST /shutdown` with the token | 202, server exits 0 | As expected |
| S18 | SIGTERM to an idle server | Exits 143 at once, without the drain of `/shutdown` | Safe while idle |
| S19 | Second server on a port in use | **Linux: logs `Listening` without listening, never exits. macOS: both servers share the port** | Wrong |
| S20 | Security finding | Reported privately | Finding |
| S21 | SIGINT to an idle server | Exits 130 at once | As expected |

## Known defects of the analyzers

Found by the [initial static analysis](static-analysis-initial.md), tracked in their repositories:

| | Scenario | Result | Issue |
|---|---|---|---|
| K1 | The concurrency analyzer on CoreTrace's own code, default `--jobs`, macOS | Crashes with SIGBUS: Clang's stack overflows on a worker thread | [coretrace-compiler#204](https://github.com/CoreTrace/coretrace-compiler/issues/204) |
| K2 | The stack analyzer v0.22.0 on CoreTrace's own code | Does not finish after 42 minutes; `ctrace` cannot stop it | [#169](https://github.com/CoreTrace/coretrace/issues/169), [#172](https://github.com/CoreTrace/coretrace/issues/172) |
| K3 | A compilation database with `-Werror`, analyzer libc++ warning | The unit fails to compile, exit 3 | [coretrace-stack-analyzer#214](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/214), [coretrace-concurrency-analyzer#199](https://github.com/CoreTrace/coretrace-concurrency-analyzer/issues/199) |

## Actions

| Problem | Scenarios | Issue |
|---|---|---|
| Interruption leaves tools running and run directories behind; no drain on signals | C31–C34, S18 | [#188](https://github.com/CoreTrace/coretrace/issues/188) |
| Two cases pass the gate: no analysis selected, unreadable tool output | C27, C35 | [#189](https://github.com/CoreTrace/coretrace/issues/189) |
| File names starting with `-`, overlapping selections, missing inputs found late | C07, C08, C23, C36, C37 | [#190](https://github.com/CoreTrace/coretrace/issues/190) |
| Port in use not detected; HTTP statuses, protocol and unknown parameters | S04, S05, S19 | [#191](https://github.com/CoreTrace/coretrace/issues/191) |
| What a request can make the server do | S14, S15, S20 | Private security advisory |

This campaign is run again at the end of the project, on the final commit, to check these
fixes; the scenarios that found a problem then become tests.
