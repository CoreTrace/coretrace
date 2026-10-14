# Pull requests and reviews

How changes reach the five repositories of level 1 (see [Open-source scope](../open-source-scope.md)),
who reviews them, and the record of it.

| | |
|---|---|
| Date | 2026-10-14 |
| Scope | `coretrace`, `coretrace-stack-analyzer`, `coretrace-concurrency-analyzer`, `coretrace-runtime-analyzer`, `coretrace-gui`; Dependabot pull requests excluded |
| Rule | "How we review" in [CONTRIBUTING.md](../../CONTRIBUTING.md) |

## The rule

- **A contributor's pull request is reviewed by a maintainer** before it merges, with GitHub's
  review tool (comments on the lines, then Approve or Request changes).
- **A maintainer's own pull request merges once the required checks pass.** There, CI is the
  gate, backed by the [test policy](test-policy.md): 82 tests on Ubuntu, coverage measured on every pull
  request, formatting, licenses, and the release archive run on four distributions.
- **Every change goes through a pull request.** Each repository has a ruleset on its default
  branch: pull request required, required checks, no force-push, no deletion.

| Repository | Ruleset since | Required checks |
|---|---|---:|
| `coretrace` | 2026-10-13 | 7 |
| `coretrace-gui` | 2026-10-13 | 3 |
| `coretrace-stack-analyzer` | 2026-10-14 | 10 |
| `coretrace-concurrency-analyzer` | 2026-10-14 | 7 |
| `coretrace-runtime-analyzer` | 2026-10-14 | 4 |

### Why not a second reviewer on every pull request

One maintainer writes most of the code: 323 of the 385 pull requests. Requiring another
maintainer's approval would hold each of them until someone is available, for a team that cannot
review at that pace. The team chose to spend human review on the changes that come from others,
where a maintainer knows what the contributor may not, and to make CI mandatory everywhere for
the rest. Two consequences are accepted and watched: a maintainer's mistake that no test catches
can reach `main`, and the record holds few human reviews. Every regression CI caught, and the two
that reached `main` before CI was required, are listed in [Regressions caught](regressions.md).

## The record

| Repository | Pull requests | Merged | Human reviews by someone else | Automated reviews | History |
|---|---:|---:|---:|---:|---|
| `coretrace` | 99 | 94 | 2 | 0 | [pull requests](https://github.com/CoreTrace/coretrace/pulls?q=is%3Apr) |
| `coretrace-stack-analyzer` | 102 | 99 | 2 | 20 | [pull requests](https://github.com/CoreTrace/coretrace-stack-analyzer/pulls?q=is%3Apr) |
| `coretrace-concurrency-analyzer` | 115 | 112 | 0 | 0 | [pull requests](https://github.com/CoreTrace/coretrace-concurrency-analyzer/pulls?q=is%3Apr) |
| `coretrace-runtime-analyzer` | 17 | 16 | 0 | 0 | [pull requests](https://github.com/CoreTrace/coretrace-runtime-analyzer/pulls?q=is%3Apr) |
| `coretrace-gui` | 52 | 51 | 7 | 13 | [pull requests](https://github.com/CoreTrace/coretrace-gui/pulls?q=is%3Apr) |
| **Total** | **385** | **372** | **11** | **33** | |

Automated reviews come from GitHub Advanced Security (code scanning findings on the pull request),
the Codex connector and Copilot. They are comments, not approvals.

**Pull request descriptions.** Since the audit of 2026-09-14, the maintainer's merged pull
requests in `coretrace` and the analyzers carry a description of at least 200 characters in 216
cases out of 228 (95%): what changed, why, and how it was tested. The other 12 are mostly
dependency updates and CI fixes.

**Direct pushes**, before the rulesets: none among the last 25 commits of each analyzer's `main`,
one among the last 60 of `coretrace` (`7aabc69`, the removal of a delivered TODO list), and 64
of the 114 commits in the history of `coretrace-gui`'s default branch. Since the rulesets, none is
possible.

## Notable reviews

Contributions from outside the team:

- [coretrace#166](https://github.com/CoreTrace/coretrace/pull/166), 2026-10-04: a fix for
  `isMangled` reading past a string slice, by a first-time contributor. The review confirmed that
  the new slice test fails without the fix, pushed a follow-up commit on the contributor's branch
  (assertions, includes, a comment on why the test fails) and explained each change. The
  formatting check had blocked the first version.
- [coretrace#168](https://github.com/CoreTrace/coretrace/pull/168), 2026-10-08: the same
  contributor's follow-up, after an invitation to take [#167](https://github.com/CoreTrace/coretrace/issues/167).
  The review corrected an inaccuracy that the issue itself had introduced, about which call could
  throw.
- [coretrace#165](https://github.com/CoreTrace/coretrace/pull/165), 2026-10-02: approved with the
  review tool.
- [coretrace#30](https://github.com/CoreTrace/coretrace/pull/30), 2026-03-27: declined, with the
  reason (a broader pull request covered the same issue) and an invitation to contribute again.

Within the team:

- [coretrace-stack-analyzer#69](https://github.com/CoreTrace/coretrace-stack-analyzer/pull/69):
  changes requested on the Windows port, because its CI path relaxed the regression harness
  enough to go green while broken. Not merged.
- [coretrace-stack-analyzer#89](https://github.com/CoreTrace/coretrace-stack-analyzer/pull/89),
  2026-09-06: approved after checking the tree rather than the description: both call sites of
  the compiler are under the lock, and the lock order is safe.
- `coretrace-gui`: Cédric Roulof reviews the others' pull requests, for example
  [#69](https://github.com/CoreTrace/coretrace-gui/pull/69) and
  [#70](https://github.com/CoreTrace/coretrace-gui/pull/70) (approved), and
  [#77](https://github.com/CoreTrace/coretrace-gui/pull/77) (comments).

Pull requests that CI refused are in [Regressions caught](regressions.md), and in
`coretrace-gui` [#89](https://github.com/CoreTrace/coretrace-gui/pull/89): a release whose three
version numbers disagreed, refused before anything was published.

## Next review of this note

At the end of the project: the same figures from 2026-10-14 on, the reviews of the contributions
that the November and December calls bring, and whether the rule still fits the team.
