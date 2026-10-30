# Contributions and welcome tickets

Every welcome ticket the project opened, and every contribution it received from outside the
team, with the exchanges and the decision taken. Contributions from the team are not listed;
the [pull request history](quality/pull-requests-and-reviews.md) covers them.

| | |
|---|---|
| Updated | 2026-10-16 |
| Scope | The repositories of level 1 of the [open-source scope](open-source-scope.md) |
| Times | UTC, as GitHub records them |

## Welcome tickets

Issues labelled `good first issue`: each says what to change, where, and how to check it
([all of them](https://github.com/search?q=org%3ACoreTrace+label%3A%22good+first+issue%22&type=issues)).

### October 2026

| Ticket | Opened | Taken by | Result |
|---|---|---|---|
| [coretrace#145](https://github.com/CoreTrace/coretrace/issues/145) Include guards in `mangle.hpp` | 2026-10-01 | PasVegan | Merged, [#147](https://github.com/CoreTrace/coretrace/pull/147), the same day |
| [coretrace#146](https://github.com/CoreTrace/coretrace/issues/146) Unit tests for `ctrace_tools::mangle` | 2026-10-01 | tecnolgd | Merged, [#166](https://github.com/CoreTrace/coretrace/pull/166), 2026-10-04 |
| [coretrace#163](https://github.com/CoreTrace/coretrace/issues/163) Remove the unused `colors.hpp` | 2026-10-02 | thanvish21 | Merged, [#165](https://github.com/CoreTrace/coretrace/pull/165), the same day |
| [coretrace#164](https://github.com/CoreTrace/coretrace/issues/164) Unit tests for `RunDirectory` | 2026-10-02 | tecnolgd, assigned 2026-10-13 | Merged, [#197](https://github.com/CoreTrace/coretrace/pull/197), 2026-10-16 |
| [coretrace#167](https://github.com/CoreTrace/coretrace/issues/167) Move `isMangled` out of the public header | 2026-10-04 | tecnolgd | Merged, [#168](https://github.com/CoreTrace/coretrace/pull/168), 2026-10-08 |
| [coretrace#175](https://github.com/CoreTrace/coretrace/issues/175) Remove the external CLI11 download option | 2026-10-13 | — | Open |
| [coretrace#177](https://github.com/CoreTrace/coretrace/issues/177) `const` reference in `findStackAnalyzerSection` | 2026-10-13 | — | Open |
| [coretrace-gui#81](https://github.com/CoreTrace/coretrace-gui/issues/81) Light and dark themes | 2026-10-01 | — | Open |
| [coretrace-gui#85](https://github.com/CoreTrace/coretrace-gui/issues/85) Breadcrumb contrast | 2026-10-09 | — | Open |
| [coretrace-gui#86](https://github.com/CoreTrace/coretrace-gui/issues/86) Keyboard access to the "Analyser" menu | 2026-10-09 | — | Open |

The five tickets of `coretrace` opened from 2026-10-01 to 2026-10-04 were all taken by people
outside the team, and all five are merged: four within four days of being opened, the fifth,
#164, three days after it was assigned. One contributor, tecnolgd, came back for a second
ticket, then a third.

### March 2026

The first tickets labelled `good first issue`
([coretrace#26](https://github.com/CoreTrace/coretrace/issues/26),
[#27](https://github.com/CoreTrace/coretrace/issues/27),
[#29](https://github.com/CoreTrace/coretrace/issues/29),
[coretrace-stack-analyzer#63](https://github.com/CoreTrace/coretrace-stack-analyzer/issues/63),
[coretrace-concurrency-analyzer#1](https://github.com/CoreTrace/coretrace-concurrency-analyzer/issues/1))
were all done by the maintainer. One of them, #29, also drew an outside pull request (#30,
below).

## Contributions received

| Date | From | Contribution | First answer | Exchanges | Decision |
|---|---|---|---|---|---|
| 2026-03-27 | HuiNeng6 | [coretrace#30](https://github.com/CoreTrace/coretrace/pull/30), `--version` option | 21 min | Thanks, and the reason in the pull request | **Declined, with reasons**: [#31](https://github.com/CoreTrace/coretrace/pull/31), opened by the maintainer twelve minutes later, covered the same issue more fully, with the version taken from the build |
| 2026-10-01 | AnabolicRoo | [coretrace#130](https://github.com/CoreTrace/coretrace/issues/130), the README does not say which LLVM version | 3 h 10 | Acceptance criteria in the issue | **Accepted**: README fixed by [#131](https://github.com/CoreTrace/coretrace/pull/131) |
| 2026-10-01 | Bilelkihal | [coretrace#132](https://github.com/CoreTrace/coretrace/issues/132), `--async` does not parallelize across files | 5 min | Split into eight issues, with the order to do them in | **Accepted**: [#133](https://github.com/CoreTrace/coretrace/issues/133) to [#140](https://github.com/CoreTrace/coretrace/issues/140), all delivered by 2026-10-02 |
| 2026-10-01 | PasVegan | [coretrace#147](https://github.com/CoreTrace/coretrace/pull/147), include guards (welcome ticket #145) | 29 min | — | **Merged** as proposed |
| 2026-10-02 | Bilelkihal | A vulnerability report on server mode, sent privately as [SECURITY.md](../SECURITY.md) asks | 13 days | In the private advisory | **Accepted** on 2026-10-15; the fix is under way. The first answer came after the five business days SECURITY.md promises |
| 2026-10-02 | Dependabot | [coretrace#162](https://github.com/CoreTrace/coretrace/pull/162), upgrade `pyyaml` in the test project | 2 h 24 | The reason in the pull request | **Declined, with reasons**: the test project pins a vulnerable version on purpose, so that `ctrace` must report it ([Regressions caught](quality/regressions.md), R5) |
| 2026-10-02 | thanvish21 | [coretrace#165](https://github.com/CoreTrace/coretrace/pull/165), remove `colors.hpp` (welcome ticket #163) | 2 h 19 | Approved with the review tool | **Merged** as proposed |
| 2026-10-04 | tecnolgd | [coretrace#166](https://github.com/CoreTrace/coretrace/pull/166), unit tests for `mangle` and a fix for `isMangled` (welcome ticket #146) | 10 h | The review confirmed that the new test fails without the fix, and explained each change of a follow-up commit pushed on the contributor's branch | **Merged with modifications, argued** in the pull request |
| 2026-10-08 | tecnolgd | [coretrace#168](https://github.com/CoreTrace/coretrace/pull/168), move `isMangled` to its source file (welcome ticket #167) | 3 h | A follow-up commit, explained: it restored the documentation comment and corrected a wording that the issue itself had got wrong | **Merged with modifications, argued** in the pull request |
| 2026-10-13 | tecnolgd | Asked for [coretrace#164](https://github.com/CoreTrace/coretrace/issues/164) | Same day | — | **Assigned** |
| 2026-10-15 | tecnolgd | [coretrace#197](https://github.com/CoreTrace/coretrace/pull/197), unit tests for `RunDirectory` (welcome ticket #164) | 20 h | Thanks in the pull request, then a follow-up commit pushed on the contributor's branch, explained in its message: the location is checked as a directory rather than as a string prefix, and the test follows the naming of the other tests | **Merged with modifications** |

Every contribution got a first answer, and every decision is written in the pull request or the
issue it concerns. Of the nine contributions of October, eight were accepted; the ninth, from a
bot, was declined with its reason.

## How contributions are handled

As [CONTRIBUTING.md](../CONTRIBUTING.md) describes: a first answer within three business days, a
review with GitHub's review tool, and a reason written for any change requested or any refusal.
Decisions that go beyond one pull request follow [GOVERNANCE.md](../GOVERNANCE.md).
