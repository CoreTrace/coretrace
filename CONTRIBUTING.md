# Contributing to ctrace

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
