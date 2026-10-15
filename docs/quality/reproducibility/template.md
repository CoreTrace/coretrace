# Reproducibility test of YYYY-MM-DD

Run of the [reproducibility protocol](../reproducibility-protocol.md).

| | |
|---|---|
| Date | YYYY-MM-DD, from HH:MM to HH:MM |
| Tester | Name or pseudonym, with their consent; otherwise their profile only |
| Profile | Studies or job, experience with C/C++, Git, CMake; never installed `ctrace` before |
| Observer | Maintainer's name |
| Machine | OS and version, processor, memory, network |
| Commit tested | `git rev-parse --short HEAD` of their clone |

## Results

| # | Task | Start | End | Duration | Done | Notes |
|---|---|---|---|---|---|---|
| T1 | Understand what `ctrace` does | | | | yes / no | |
| T2 | Quick start with the release archive (Linux) | | | | yes / no / n.a. | |
| T3 | Build from source | | | | yes / no | |
| T4 | Test suite | | | | yes / no | |
| T5 | Analyze a file, SARIF report | | | | yes / no | |
| T6 | Find a first contribution | | | | yes / no | |
| | **Total** | | | | | |

## Blockers

One line per blocker: anything that stopped the tester or made them search for more than a few
minutes, and every question they asked.

| # | Task | When | What happened | Time lost | How it ended (alone, hint given) | Cause | Correction |
|---|---|---|---|---|---|---|---|
| B1 | | | | | | Documentation / code / environment | PR or issue link |

## Debrief

- Hardest step:
- Looked for and not found:
- First change to the documentation:
- Confidence to install again alone (out of 5):
- Confidence to contribute (out of 5):

## Corrections

What was changed after this test, with links, and what is left open.

## Conclusion

In two or three sentences: can someone outside the team install, build, test and find a first
contribution with the documentation alone, in how long, and what still stands in the way.
