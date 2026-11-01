# Contributions and welcome tickets

Every welcome ticket the project opened, and every contribution it received from outside the
team, with the exchanges and the decision taken. Contributions from the team are not listed;
the [pull request history](quality/pull-requests-and-reviews.md) covers them.

| | |
|---|---|
| Updated | 2026-11-01 |
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
| [coretrace#175](https://github.com/CoreTrace/coretrace/issues/175) Remove `USE_EXTERNAL_CLI11` | 2026-10-13 | glitchjay-hub, assigned 2026-10-30 | Merged, [#217](https://github.com/CoreTrace/coretrace/pull/217), 2026-10-31 |
| [coretrace#177](https://github.com/CoreTrace/coretrace/issues/177) `const` reference in `findStackAnalyzerSection` | 2026-10-13 | glitchjay-hub | Merged, [#214](https://github.com/CoreTrace/coretrace/pull/214), 2026-10-29 |
| [coretrace-gui#81](https://github.com/CoreTrace/coretrace-gui/issues/81) Light and dark themes | 2026-10-01 | — | Open |
| [coretrace-gui#85](https://github.com/CoreTrace/coretrace-gui/issues/85) Breadcrumb contrast | 2026-10-09 | — | Open |
| [coretrace-gui#86](https://github.com/CoreTrace/coretrace-gui/issues/86) Keyboard access to the "Analyser" menu | 2026-10-09 | — | Open |

The five tickets of `coretrace` opened from 2026-10-01 to 2026-10-04 were all taken by people
outside the team, and all five are merged: four within four days of being opened, the fifth,
#164, three days after it was assigned. One contributor, tecnolgd, came back for a second
ticket, then a third, then asked for #173, which is not a welcome ticket.

The two tickets opened on 2026-10-13 waited nearly two weeks, then were both taken by
glitchjay-hub, who came back for #175 after #177. #177 drew three pull requests within two days,
before the requests on the issue had an answer: the ticket went to the first to ask, and the two
other pull requests were closed as duplicates, with their reasons. All seven welcome tickets of
`coretrace` are now merged; the three of the GUI are still open.

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
| 2026-10-26 | glitchjay-hub | Asked for [coretrace#177](https://github.com/CoreTrace/coretrace/issues/177) | 2 days 16 h | One answer on the issue to both requests: the ticket goes to the first to ask | **Accepted** as the first to ask |
| 2026-10-26 | glitchjay-hub | [coretrace#214](https://github.com/CoreTrace/coretrace/pull/214), `LoadContext` by `const` reference (welcome ticket #177) | 2 days 14 h | Opened two hours after the request, before its answer; the answer on the issue announced the review, and the pull request was thanked once merged | **Merged** as proposed; the title became a Conventional Commit at the merge |
| 2026-10-27 | hazzyadil | Asked for [coretrace#177](https://github.com/CoreTrace/coretrace/issues/177) | 1 day 14 h | The same answer, which offered [#175](https://github.com/CoreTrace/coretrace/issues/175) instead | **Declined, with reasons**: another contributor had asked first |
| 2026-10-27 | tecnolgd | Asked for [coretrace#173](https://github.com/CoreTrace/coretrace/issues/173) | 2 days 13 h | — | **Assigned** |
| 2026-10-27 | Mayur-dambale | [coretrace#215](https://github.com/CoreTrace/coretrace/pull/215), the same change (welcome ticket #177), opened without asking on the issue | 2 days 16 h | Closed on 2026-10-28 as a duplicate of #214; the reasons followed in the pull request on 2026-10-30, with what a review would have raised: the format check, the issue link, the checklist, the commit type | **Declined, with reasons** |
| 2026-10-27 | hazzyadil | [coretrace#216](https://github.com/CoreTrace/coretrace/pull/216), the same change (welcome ticket #177), opened before the request on the issue had an answer | 1 day 1 h | The answer on the issue announced one in the pull request; closed the same day as a duplicate of #214, and the reasons followed there on 2026-10-30, with advice on waiting for the assignment, the commit email and the title | **Declined, with reasons** |
| 2026-10-29 | glitchjay-hub | Asked for [coretrace#175](https://github.com/CoreTrace/coretrace/issues/175) | 22 h | — | **Assigned** |
| 2026-10-30 | hazzyadil | Asked for [coretrace#176](https://github.com/CoreTrace/coretrace/issues/176) | 40 min | Pointers in the issue, including a second place to change that the issue did not name | **Assigned** |
| 2026-10-30 | glitchjay-hub | [coretrace#217](https://github.com/CoreTrace/coretrace/pull/217), remove `USE_EXTERNAL_CLI11` (welcome ticket #175) | 19 h 50 | Approved with the review tool, which explained a follow-up commit pushed on the contributor's branch: it restored two documents the maintainers keep, which the issue's acceptance criterion had led the contributor to edit | **Merged with modifications, argued** in the pull request |
| 2026-10-31 | Vamsi-2581 | [coretrace-stack-analyzer#257](https://github.com/CoreTrace/coretrace-stack-analyzer/pull/257), the GitHub Action diagnoses the Ubuntu and LLVM versions it does not support | 9 h 13 | Thanks, and four follow-up commits pushed on the contributor's branch, explained in the pull request: among other changes, the action accepts LLVM 20 only, the version the analyzer builds with | **Merged with modifications, argued** in the pull request |

Every contribution got a first answer, and every decision is written in the pull request or the
issue it concerns. Of the fourteen contributions of October, eleven were accepted. Three were
declined with their reason: one from a bot, and two pull requests that duplicated #214, for a
ticket another contributor had asked for first.

## How contributions are handled

As [CONTRIBUTING.md](../CONTRIBUTING.md) describes: a first answer within three business days, a
review with GitHub's review tool, and a reason written for any change requested or any refusal.
Decisions that go beyond one pull request follow [GOVERNANCE.md](../GOVERNANCE.md).
