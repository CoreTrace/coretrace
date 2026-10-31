# Changelog

Notable changes are grouped by release. See [all releases](https://github.com/CoreTrace/coretrace/releases) for older versions.

## Unreleased

- **Exit codes:** a run that checks nothing no longer passes the gate. With no analysis
  selected (`--static`, `--dyn` or `--invoke`), `ctrace` exits 1. A tool whose findings
  CoreTrace cannot read makes the analysis incomplete (exit code 3) instead of clean: `ikos`,
  so every `--static` run on C or C++ sources; `flawfinder` printing no SARIF; `cppcheck` or
  `tscancode` reporting in a format CoreTrace does not parse. The server's `result.gate` names
  them in `unread_tools`.
- **Interrupting `ctrace`:** Ctrl+C, SIGINT or SIGTERM stops the external tools of a run and
  removes their run directories, instead of leaving them behind, and starts no other tool; the
  run gives no report and ends by the signal (130 or 143 in a shell). What a tool leaves running
  in the background ends with it. A server drains as `POST /shutdown` does before it
  ends. A second interrupt quits at once.
- **SARIF:** a rule of the merged log carries the title, description and documentation link its
  tool gives, so that GitHub code scanning can explain the finding; flawfinder's rule titles and
  CWE links come first, the analyzers' when they describe their rules.
- **SARIF:** the related locations and the confidence a tool writes in its SARIF reach the merged
  log (the runtime analyzer's allocation site, the Python analyzer's sink), and a result's
  message links to its related locations, which GitHub code scanning then shows as links.
- **Inputs:** an input file, a compilation database or a source it lists that does not exist
  stops the run before any tool starts, with exit code 1 and a message naming it
  (`Input file not found`, `Compile database not found`, `Source file not found`), instead of
  each tool failing on it (exit code 3). A compilation database is checked even beside
  `--input`. The server answers `InvalidInput`.
- **Tool selection:** a tool selected twice, by `--static` or `--dyn` and by `--invoke`, runs
  once, and its findings are counted once.
- **macOS:** the disk image needs macOS 15 or newer, instead of 14. GitHub retires its macOS 14
  runners on 2026-11-02, and the image ships Homebrew libraries built for the macOS of the
  runner that makes it, now macOS 15.

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
