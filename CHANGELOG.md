# Changelog

Notable changes are grouped by release. See [all releases](https://github.com/CoreTrace/coretrace/releases) for older versions.

## Unreleased

- **Exit codes:** a run that checks nothing no longer passes the gate. With no analysis
  selected (`--static`, `--dyn` or `--invoke`), `ctrace` exits 1. A tool whose findings
  CoreTrace cannot read makes the analysis incomplete (exit code 3) instead of clean: `ikos`,
  so every `--static` run; `flawfinder` printing no SARIF; `cppcheck` or `tscancode` reporting
  in a format CoreTrace does not parse. The server's `result.gate` names them in
  `unread_tools`.

## v0.85.0

- Releases include a macOS disk image, for Apple silicon with macOS 14 or newer.
- The stack analyzer is v0.23.1: CoreTrace's own code is analyzed in about 1.5 minutes instead
  of 3 in a Release build.
- `-j/--jobs` runs the tools of several files at the same time; `async` is deprecated.
- **HTTP server, for clients:** a request must give `proto: coretrace-1.0` and only known
  parameters, and errors are answered with their HTTP status (400, 403 or 500) instead of 200.
  A second server on an address and port in use exits with code 1.
- A file whose name starts with `-` is analyzed as a file by every tool, instead of being read
  as an option, and its findings are spelled `./-F.c`.
- Added repository contribution and community documentation.

[Full v0.85.0 notes](https://github.com/CoreTrace/coretrace/releases/tag/v0.85.0).

## v0.84.0

- Release archives include third-party license texts.
- Fetched dependencies are pinned and release archives are attested.
- Shared libraries are stored once in the release archive.

[Full v0.84.0 notes](https://github.com/CoreTrace/coretrace/releases/tag/v0.84.0).
