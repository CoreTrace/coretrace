# Contributing to ctrace

Thanks for your interest. This guide takes you from choosing an issue to a merged pull request.

## Find something to work on

- Issues labelled
  [`good first issue`](https://github.com/CoreTrace/coretrace/issues?q=is%3Aissue%20state%3Aopen%20label%3A%22good%20first%20issue%22)
  are scoped for a first contribution: each says what to change, where, and how to check it.
- Comment on the issue to ask for it. A maintainer assigns it to you and answers your questions
  there. If you stop working on it, say so in the issue so that someone else can take it.
- For a change that no issue describes, open one first to agree on the approach. Changes that
  break users go through the process of [GOVERNANCE.md](GOVERNANCE.md).
- Report a bug or ask for a feature with the
  [issue forms](https://github.com/CoreTrace/coretrace/issues/new/choose).

A maintainer gives a first answer to every issue and pull request within **three business days**.

## Local setup

Follow the [build instructions](README.md#build) for LLVM/Clang 20 and CMake. The Python analyzer and its virtual environment are optional; enable them only when working on Python integration.

Build and run the suite for the configuration you changed:

```bash
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Run `./scripts/format-check.sh` for C++ changes; `./scripts/format.sh` applies the project format. Describe the behavior you changed, the commands you ran, and any platform limits in your pull request. Use an English Conventional Commit subject such as `fix(cli): handle missing input`.

For a suspected vulnerability, use the private reporting route in [SECURITY.md](SECURITY.md), not a public issue.

## How we review

- A pull request from a contributor is reviewed by a maintainer before it merges, with GitHub's
  review tool: comments on the lines concerned, then **Approve** or **Request changes**, so that
  every decision stays in the pull request.
- The reviewer checks that the change does what its description says, that it comes with its
  tests (for a bug fix, a test that fails without the fix), that the documentation follows, and
  that CI passes.
- A maintainer's own pull request merges once the required checks pass: there, CI is the gate.
  Its description says what changed, why, and how it was tested.
- Disagreements are settled in the review thread, which is resolved once they are.
