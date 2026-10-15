# Reproducibility test: protocol

How the project checks that someone outside the team can install `ctrace`, build it, test it and
find a first contribution with the public documentation alone. Each run of this protocol produces
a dated report in `docs/quality/reproducibility/`.

## Who

**The tester** is outside the team and has never installed `ctrace`. They know Git, the command
line and the basics of C or C++. They use their own machine, with one of:

- Ubuntu 24.04, natively or in WSL2;
- macOS 14 or newer.

These are the platforms the README supports. Windows outside WSL2 is not supported
([#54](https://github.com/CoreTrace/coretrace/issues/54)).

**The observer** is a maintainer. They watch, take notes and keep time; they do not help.

Before starting, ask the tester whether their name or pseudonym may appear in the public report;
otherwise the report describes their profile only.

## Rules

1. The tester starts from one link: <https://github.com/CoreTrace/coretrace>. They may read
   anything public (README, `CONTRIBUTING.md`, the wiki, issues) and search the web, as a real
   newcomer would.
2. The tester thinks aloud: what they look for, what they expect, what surprises them.
3. The observer does not answer questions and does not point at anything. A question asked is
   noted as a sign that the documentation does not answer it.
4. If the tester is stuck for **15 minutes** on the same step, the observer gives the smallest
   hint that unblocks them, and notes it as a blocker with the hint given.
5. The observer notes the time at the start and end of each task, and every blocker: when, what,
   time lost, and how it ended.

## Tasks

| # | Task | Done when | Expected time |
|---|---|---|---|
| T1 | Say what `ctrace` does and for whom, after reading the top of the README | They explain it in their own words | 5 min |
| T2 | Linux only: run the quick start with the release archive | `demo.c` is analyzed, the overflow is reported and the exit code is 2 | 10 min |
| T3 | Build from source, following the README's BUILD section | `./build/ctrace --help` prints the help | 30–60 min, mostly downloads |
| T4 | Run the test suite | `ctest --test-dir build --output-on-failure` passes | 5 min |
| T5 | Analyze a small C file of their choice with their build, and produce a SARIF report | The report file exists and lists the findings | 10 min |
| T6 | Find a first contribution: pick a `good first issue`, say what to change and how they would ask for it | They name the issue, the files and the steps of `CONTRIBUTING.md` | 10 min |

T6 does not ask them to write the change. If they want to go on, they can take the issue for real:
it then follows the usual process, and the report links the pull request.

Plan two hours, with the debrief.

## Debrief (10 minutes)

1. What was the hardest step, and why?
2. What did you look for and not find?
3. What would you change first in the documentation?
4. Out of 5, how confident would you be to install `ctrace` again alone? To contribute?

## After the test

1. Fill in the report from the template, within two days, while the notes are fresh.
2. For each blocker, fix the documentation or the code, or open an issue, and link it in the
   report's "Corrections" column.
3. Add the report through a pull request:
   `docs/quality/reproducibility/<date>.md`.
4. If a correction changes a step, a second tester runs the protocol again on that step.
