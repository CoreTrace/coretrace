# Coverage report of 2026-10-15

A copy of the `coverage-report` artifact that CI produced for `main`, kept here because GitHub
deletes artifacts after 90 days.

| | |
|---|---|
| Commit | `962f8dc` |
| CI run | [38686102491](https://github.com/CoreTrace/coretrace/actions/runs/38686102491) |
| Tool | llvm-cov (Clang 20), `scripts/ci/coverage.sh` |
| Files | `summary.md`; `unit.txt`, `integration.txt` and `all.txt`, the per-file reports of the unit tests, the integration tests and both |

## Summary

| Tests | Lines | Functions | Branches |
|---|---:|---:|---:|
| Unit | 64.58% | 73.11% | 62.06% |
| Integration | 79.52% | 91.90% | 66.88% |
| All | 89.60% | 96.09% | 81.38% |

## Critical path

The critical path is defined in the [test policy](../../test-policy.md): the 35 files between
the inputs and the verdict, 4,750 of the 5,518 measured lines.

| Part of the code | Files | Lines, all tests | Branches, all tests | Lines, unit tests only |
|---|---:|---:|---:|---:|
| Critical path | 35 | 4,311 / 4,750 (90.8%) | 1,359 / 1,665 (81.6%) | 72.7% |
| Server path | 5 | 567 / 699 (81.1%) | 139 / 180 (77.2%) | 6.6% |
| Other | 2 | 66 / 69 (95.7%) | 23 / 24 (95.8%) | 95.7% |

Both paths are above the floors of the test policy (critical path: 90% of lines, 80% of
branches; server path: 80% and 75%).
