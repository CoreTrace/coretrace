# Licensing

What CoreTrace's license allows, where it came from, and why every component `ctrace` uses or
ships is compatible with it.

| | |
|---|---|
| Date | 2026-10-13 |
| Scope | This repository at `f2672ac`, and the `v0.84.0` release archives |
| Method | Read the build files and the release packaging; downloaded `coretrace-v0.84.0-linux-amd64.tar.gz` (SHA-256 verified) and read every license it carries |

This note documents the project's reasoning; it is not legal advice.

## The license: Apache-2.0

CoreTrace is distributed under the [Apache License 2.0](../LICENSE), with a [`NOTICE`](../NOTICE)
file. Every source file carries `SPDX-License-Identifier: Apache-2.0`, and CI checks it.

Why it fits CoreTrace:

- **The same family as LLVM.** Every analyzer is built on LLVM/Clang, which is
  Apache-2.0 with the LLVM exception. CoreTrace and its main dependency share one set of terms.
- **An explicit patent grant.** Each contributor grants a patent license on their contribution
  (section 3), which matters for analysis techniques. The grant ends for anyone who sues over
  patents in the work.
- **Adoption in CI.** `ctrace` is meant to gate pipelines, including proprietary ones. A
  permissive license lets a company run, embed or adapt it without opening its own code.
- **Clear contribution terms.** A contribution submitted for inclusion is licensed under
  Apache-2.0 (section 5), so the project needs no separate contributor agreement.

### What it allows, requires and excludes

| | Terms |
|---|---|
| **Allows** | Use for any purpose, commercial included; reproduce, modify and distribute, as source or binary; sublicense; use the contributors' patents needed to run the code (sections 2, 3) |
| **Requires**, when distributing | Give a copy of the license (4a); mark modified files (4b); keep the copyright, patent, trademark and attribution notices of the source (4c); include the attribution notices of `NOTICE` (4d) |
| **Excludes** | Any right to the name "CoreTrace" or its logos (section 6); any warranty (section 7); any liability of the contributors (section 8) |

In practice, a company can run `ctrace` in its CI, or ship a modified version under its own
terms, as long as it keeps `LICENSE` and `NOTICE` and marks what it changed. Apache-2.0 code can
also go into a GPL-3.0 project, such as `coretrace-vscode`, but not into a GPL-2.0-only one.

## Licensing history

| Repository | Before | Since |
|---|---|---|
| `coretrace` | GPL-3.0, from the first commit (2025-03-11) | Apache-2.0, 2026-03-29 (#33, #35) |
| `coretrace-compiler` | GPL-3.0 | Apache-2.0, 2026-03-29 |
| `coretrace-stack-analyzer` | no license file, so all rights reserved (from 2025-04-07) | Apache-2.0, 2026-03-30 |

Changing a license needs the agreement of every author whose code is still in the work. Before
these changes, only two people other than Hugo Payet had authored commits on `main`:

- Arthur Mazeau (`@Arthurvroum`): 7 commits in `coretrace`, 10 in `coretrace-compiler`, 7 in
  `coretrace-stack-analyzer`;
- Cédric Roulof (`@shookapic`): 2 commits in `coretrace`.

Both confirmed in writing, on 2026-10-13, that their contributions made before the change are
licensed under Apache-2.0
([#171](https://github.com/CoreTrace/coretrace/issues/171)). Every contribution since the change
is under Apache-2.0 by section 5, for example #147, #165, #166 and #168.

## Compatibility of the dependencies

Versions are those of `v0.84.0`. The exact package versions are in `licenses/**/VERSION` in the
archive.

### Linked into `ctrace`

| Component | Version | License | How it is used | Compatible |
|---|---|---|---|---|
| coretrace-stack-analyzer | v0.22.0 | Apache-2.0 | static library | yes, same license; `LICENSE` and `NOTICE` shipped |
| coretrace-concurrency-analyzer | v0.7.1 | Apache-2.0 | static library | yes, same |
| coretrace-compiler | `866fa76` | Apache-2.0 | static library, through the analyzers | yes, same |
| coretrace-log | v1.0.0 | MIT | static library | yes, permissive |
| cpp-httplib | v0.14.3 | MIT | header | yes, permissive |
| nlohmann/json | v3.11.3 | MIT | header | yes, permissive |
| CLI11 | 2.5.0 | BSD-3-Clause | header copied into `external/CLI11.hpp` | yes, permissive; its notice stays in the header and is shipped as `licenses/CLI11/LICENSE` |
| LLVM, Clang | 20.1.8 | Apache-2.0 WITH LLVM-exception | shared libraries in `lib/`, Clang headers in `lib/clang/` | yes, same family |

### Libraries shipped in `lib/` (Ubuntu 22.04 packages)

| Package | Version | License | Compatible |
|---|---|---|---|
| libstdc++6, libgcc-s1 | 16-20260315 | GPL-3.0-or-later WITH GCC-exception-3.1 | yes: `ctrace` is compiled by GCC 13, so the GCC Runtime Library Exception lets it ship with these runtimes under its own terms |
| libicu70 | 70.1 | ICU (MIT-style) | yes; the GPL-3 entry of the Debian file covers only the packaging (`debian/*`) |
| libxml2 | 2.9.13 | MIT, ISC | yes |
| libzstd1 | 1.4.8 | BSD-3-Clause or GPL-2.0, at the user's choice | yes, under the BSD option |
| zlib1g | 1.2.11 | Zlib | yes |
| liblzma5 | 5.2.5 | public domain | yes; its GPL entries cover scripts and tools not shipped |
| libffi8 | 3.4.2 | MIT | yes |
| libedit2 | 3.1-20210910 | BSD-3-Clause | yes |
| libtinfo6 | 6.3 | X11 (MIT-style) | yes |
| libbsd0 | 0.11.5 | BSD-2/3-Clause, ISC, Beerware, a few BSD-4-Clause files | yes; the 4-clause files only ask for an acknowledgement in advertising that mentions their features |
| libmd0 | 1.0.4 | BSD-2/3-Clause, ISC, public domain | yes |

### Tools shipped in `libexec/` (separate executables)

| Component | Version | License | Compatible |
|---|---|---|---|
| coretrace-runtime-analyzer, with its `cc` and `libcompilerlib` | v0.1.0 | Apache-2.0 | yes, same |
| coretrace-python-analyzer | v0.12.0 | Apache-2.0 | yes, same |
| Nuitka runtime, compiled into the Python analyzer | 4.2.2 | AGPL-3.0 with the Nuitka Runtime Library Exception 1.0 | yes: the exception lets the compiled program be conveyed under any terms. Only the Nuitka compiler itself stays AGPL, and it is not shipped |
| Python runtime | 3.12.14 | PSF-2.0 | yes |
| OpenSSL | 3.5.5 | Apache-2.0 | yes |
| expat, libffi | 2.5.0, 3.4.2 | MIT | yes |
| bzip2 | 1.0.8 | bzip2-1.0.6 (BSD-style) | yes |
| xz | 5.2.5 | public domain | yes |
| mpdecimal | 2.5.1 | BSD-2-Clause | yes |

### External tools, neither shipped nor linked

| Tool | License | Why it does not affect CoreTrace's license |
|---|---|---|
| cppcheck | GPL-3.0-or-later | `ctrace` finds these tools through `PATH`, starts them as separate processes and reads their output: separate programs, not one combined work. No release ships them |
| flawfinder | GPL-2.0-or-later | same; `ENABLE_FLAWFINDER` builds a separate executable |
| TscanCode (CoreTrace fork) | GPL-3.0 | same; `ENABLE_TSCANCODE` builds a separate executable |
| IKOS | NOSA-1.3 | same; the build module is not included |

If a release ever ships one of these tools, it must come with its license and, for the GPL ones,
the offer of their source code. The packaging enforces the first part: see below.

## How it is checked

- **On every pull request** (a required check of `main`): `license-compliance` checks that
  `LICENSE` is Apache-2.0 and that every source file has its SPDX header, except vendored code.
- **In the test suite** (`ctest`, run by CI): `ctrace_install_ships_third_party_licenses` checks
  that the install ships the `LICENSE` and `NOTICE` of the three Apache components and the MIT
  and BSD texts; the license collection and the packaging check have their own tests.
- **At packaging time**: `scripts/release/third-party.txt` maps each shipped file to its license,
  and `check-licenses.sh` fails the packaging on any library or executable without a reviewed
  entry. `collect-licenses.sh` copies each package's license with its exact version.
- **Dependencies are pinned** to a tag or commit and a SHA-256
  (`check-pinned-dependencies.sh`), so the license reviewed is the one shipped. Each release
  archive has a build provenance attestation.

## Gaps and actions

1. **Done: the release archive now ships CoreTrace's own `NOTICE`**, next to `LICENSE`
   ([#170](https://github.com/CoreTrace/coretrace/pull/170)), from the release after `v0.84.0`.
2. **Done: consent to the relicensing.** Arthur Mazeau and Cédric Roulof confirmed it in
   [#171](https://github.com/CoreTrace/coretrace/issues/171) on 2026-10-13.
3. **`NOTICE` says "Copyright 2026 Hugo Payet".** The code dates from 2025 and has other
   authors: "Copyright 2025-2026 Hugo Payet and the CoreTrace contributors", with a pointer to
   `AUTHORS.md`, is more accurate.
4. **The organization profile is out of date.** Its table marks `coretrace-runtime-analyzer` and
   `coretrace-tool-template` "Not declared" (both are now Apache-2.0) and `coretrace-TscanCode`
   "Not identified" (GPL-3.0). `coretrace-web` and `coretrace-ci-consumer-demo` are public
   without a license, so nobody may reuse them.
