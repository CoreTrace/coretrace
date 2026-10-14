# Governance

Who decides what in CoreTrace, how decisions are made and recorded, and what the project
commits to as it becomes the open core of a commercial service.

This document applies to the repositories of level 1 and 2 of the
[open-source scope](docs/open-source-scope.md): `coretrace`, the analyzers, the compiler library,
the desktop interface and their companions. It was adopted on 2026-10-14.

## Roles

| Role | Who | Can |
|---|---|---|
| **User** | Anyone who runs CoreTrace | Report bugs, ask for features, take part in discussions |
| **Contributor** | Anyone who opens a pull request | Propose changes; see [CONTRIBUTING.md](CONTRIBUTING.md) |
| **Maintainer** | Listed below | Review and merge pull requests, triage issues, cut releases of their repositories |
| **Lead maintainer** | Hugo Payet ([@SizzleUnrlsd](https://github.com/SizzleUnrlsd)) | Everything a maintainer can, in every repository; makes the final decision when discussion does not settle a question |

### Maintainers

| Maintainer | Repositories |
|---|---|
| Hugo Payet ([@SizzleUnrlsd](https://github.com/SizzleUnrlsd)), lead | All |
| Cédric Roulof ([@shookapic](https://github.com/shookapic)) | `coretrace-gui` |

## How decisions are made

### Everyday changes

A pull request merges when its required checks pass and, for a contributor's pull request, when a
maintainer has approved it (see "How we review" in [CONTRIBUTING.md](CONTRIBUTING.md)). The pull
request is the record of the decision.

### Significant changes

These are proposed in an issue before any pull request:

- a change that breaks users: a command-line option, a configuration key, an exit code, the
  `coretrace/v1` SARIF fingerprint, or the `coretrace-1.0` HTTP protocol;
- a new analyzer or tool, or the removal of one;
- a new runtime dependency, or a change of license of a dependency;
- a change of supported platform;
- a change to this document.

The issue stays open for comments for at least **7 days** (14 for a change to this document).
If no maintainer or contributor objects with reasons, the proposal is accepted. If someone does,
the discussion continues in the issue; when it does not converge, the lead maintainer decides
and writes the decision and its reasons in the issue before closing it.

### Disagreement on a pull request

It is settled in the review thread. When a contributor and a maintainer disagree, the lead
maintainer decides and says why in the thread. A declined pull request is closed with its reason,
as in [coretrace#30](https://github.com/CoreTrace/coretrace/pull/30).

### Releases

Each repository is versioned on its own, with semantic versioning; components in `0.x` may still
change incompatibly between minor versions. A maintainer of the repository cuts a release from
its default branch once the required checks pass. Release notes list what changed for users.

## Becoming a maintainer

A contributor is invited to become a maintainer of a repository by the lead maintainer, when
they have:

- had several substantial pull requests merged in that repository over at least two months;
- reviewed others' pull requests or helped in issues;
- shown that they follow this document and the [Code of Conduct](CODE_OF_CONDUCT.md).

The invitation is announced in an issue; once accepted, the maintainer is added to the table above
and to [AUTHORS.md](AUTHORS.md). A maintainer may step down at any time. A maintainer with no
activity for twelve months is moved to an "emeritus" list, after being asked.

## Commitments

CoreTrace is the open core of a hosted analysis service, the CoreTrace platform
([app.coretrace.fr](https://app.coretrace.fr)), developed separately and not open source.

- **Open code stays open.** What is published under Apache-2.0 stays under Apache-2.0. A feature
  of an open repository is not moved to the private service.
- **Contributions stay the contributors'.** They are licensed under Apache-2.0 by section 5 of the
  license; the project asks for no contributor license agreement and no transfer of rights.
- **The boundary, today.** Open: the analyzers, `ctrace`, its report formats and the desktop
  interface. The service: running analyses on managed infrastructure, organizations, quotas and
  billing.
- **Decisions about the open repositories are made in the open**, in their issues and pull
  requests, under the rules above.
- The name "CoreTrace" and its logo are not licensed by Apache-2.0 (section 6): a fork must use
  another name.

## Code of conduct and security

Everyone taking part follows the [Code of Conduct](CODE_OF_CONDUCT.md). Vulnerabilities are
reported privately, as [SECURITY.md](SECURITY.md) explains.
