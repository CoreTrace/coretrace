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
