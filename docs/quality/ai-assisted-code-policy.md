# AI-assisted code policy

How `ctrace` keeps its technical and security debt under control when code is written faster,
with or without an AI tool: what a pull request must satisfy, how debt is repaid, and the
indicators that show whether it works.

| | |
|---|---|
| Date | 2026-11-20 |
| Applies to | Every pull request of this repository |
| Related | [AGENTS.md](../../AGENTS.md) (rules for coding agents), [technical debt register](technical-debt.md), [debt baseline](debt-baseline-2026-11-20.md), [test policy](test-policy.md) |

## Principle

An AI tool changes how fast code arrives, not what the code must satisfy. The rules below are
the same for every pull request, and the author answers for every line of it, however it was
written. Nothing here guarantees the absence of debt: the policy measures it, bounds what a
change may add, and reserves time to repay it.

## Rules

1. **Declaring AI assistance is optional.** An author who used an AI tool may tick the box of
   the pull request template; a maintainer then adds the `ai-assisted` label. Commits carry no
   mark: no trailer, no co-author, no tool name. Nobody is asked to prove how code was written.
2. **The author answers for the change.** They have read the whole diff, can explain each part
   of it, and know where the code comes from: nothing copied from a source whose license is
   unknown or incompatible with Apache-2.0.
3. **Review.** A pull request of the lead maintainer merges once the required checks pass.
   Every other pull request, from a contributor or from another maintainer, assisted or not,
   merges only after a maintainer approved it with GitHub's review tool.
4. **No merge without green gates.** The required checks of `main` pass: build with `-Werror`,
   tests, format, license headers, pinned dependencies. A gate is never weakened to let a
   change through; a gate that is wrong is fixed in its own pull request.
5. **A change adds no debt.** It comes with its tests, breaks no invariant of
   [AGENTS.md](../../AGENTS.md), and does not lower the coverage of the files it touches. Debt
   noticed elsewhere is recorded as an issue, not fixed in passing.
6. **20% of each iteration repays debt.** One fifth of the time goes to issues labelled
   `severity/*` or `refactor`, critical first, then major debt in the area the next roadmap
   item touches. A security fix does not count against this budget: it comes before everything
   else.
7. **A coding agent follows [AGENTS.md](../../AGENTS.md).** It commits, pushes, or opens an
   issue or a pull request only when a maintainer asks, and never merges.

Rule 3 is stricter than [GOVERNANCE.md](../../GOVERNANCE.md) today, which lets any maintainer
merge their own pull request on the required checks alone. It takes effect once
`GOVERNANCE.md` and `CONTRIBUTING.md` are changed, by the process `GOVERNANCE.md` sets for its
own changes.

## Indicators

Measured at each debt review, with the same commands, so that two measures compare.

| | Indicator | How it is measured | On 2026-11-20 |
|---|---|---|---|
| 1 | Static analysis findings | cppcheck and clang-tidy on the 22 translation units, with the protocol of the [debt baseline](debt-baseline-2026-11-20.md) | 69 findings, 0 at error level (cppcheck: 18 style; clang-tidy: 51 warnings) |
| 2 | Debt added and repaid | Issues labelled `severity/*` or `refactor`, opened and closed in the period | September: 30 opened, 29 closed. October: 21 opened, 14 closed. November: none. In all, 51 recorded, 43 repaid, 8 open |
| 3 | Time to fix a vulnerability | Days from a report (security advisory) or an alert (dependency scanner) to the release that fixes it; median over the period | Not measurable yet: too few events for a median. As a reference, a debt issue closes in a median of 4.7 days (critical), 4.9 (major) and 5.6 (minor) |
| 4 | Share of AI-assisted pull requests | Merged pull requests labelled `ai-assisted`, over all merged pull requests of the period | 0 of 121: the label did not exist. From now on, a lower bound, since declaring is optional |

```bash
# 2. Debt opened and closed, by month
gh issue list --state all --limit 500 --json labels,createdAt,closedAt \
  --jq '[.[] | select([.labels[].name] | any(startswith("severity/") or . == "refactor"))]
        | (map(.createdAt[0:7]) | group_by(.) | map("opened \(.[0]) \(length)")),
          (map(select(.closedAt) | .closedAt[0:7]) | group_by(.) | map("closed \(.[0]) \(length)"))
        | .[]'

# 4. Share of AI-assisted pull requests
gh pr list --state merged --limit 500 --label ai-assisted --json number --jq length
gh pr list --state merged --limit 500 --json number --jq length
```

What the indicators are read for:

- **1 goes down, or stays flat while the code grows.** A rise after a period of many assisted
  pull requests is the signal this policy exists to catch.
- **2: repaid stays close to added.** October repaid 14 for 21 added: the 20% budget of rule 6
  answers that gap.
- **3 stays within the response targets** of [SECURITY.md](../../SECURITY.md).
- **4 is context for the three others**, not a target: it is neither good nor bad in itself.

## Limits

- **"Guarantee" is not absolute.** The policy bounds and reduces debt and shows its trend; it
  cannot prove there is none.
- **AI-written code cannot be told apart.** Rule 1 relies on what authors declare, and
  declaring is optional: indicator 4 is a minimum, never the real share. That is why no rule
  depends on it.
- **The lead maintainer's pull requests have no second reader.** There, the gates of rule 4 are
  the only check, as [Pull requests and reviews](pull-requests-and-reviews.md) already
  explains for a team of this size.
- **Counts are proxies.** A finding count depends on the tools and the checks chosen; the
  number of issues depends on how carefully debt is recorded. Only measures made the same way
  compare.
- **Technical and security debt are ranked apart.** A vulnerability goes through
  [SECURITY.md](../../SECURITY.md) and comes before any refactor, whatever the budget says.
- **One measure is a starting point.** The figures above are a baseline; the trend needs the
  next reviews.
- **AI is also part of the answer**: it drafts tests, reviews diffs and repays recorded debt.
  [AGENTS.md](../../AGENTS.md) gives an agent the same rules as a contributor.
