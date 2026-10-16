# Changelog

Notable changes are grouped by release. See [all releases](https://github.com/CoreTrace/coretrace/releases) for older versions.

## Unreleased

- Added repository contribution and community documentation.
- Releases include a macOS disk image, for Apple silicon with macOS 14 or newer.
- **HTTP server, for clients:** a request must give `proto: coretrace-1.0` and only known
  parameters, and errors are answered with their HTTP status (400, 403 or 500) instead of 200.
  A second server on an address and port in use exits with code 1.

## v0.84.0

- Release archives include third-party license texts.
- Fetched dependencies are pinned and release archives are attested.
- Shared libraries are stored once in the release archive.

[Full v0.84.0 notes](https://github.com/CoreTrace/coretrace/releases/tag/v0.84.0).
