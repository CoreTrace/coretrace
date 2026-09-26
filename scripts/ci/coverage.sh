#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# coverage.sh <build directory> <output directory>
#
# Measures how much of CoreTrace's own code (src/, include/, main.cpp) the tests run, from a
# build configured with -DCORETRACE_COVERAGE=ON. The unit tests and the integration tests (the
# ctest labels, plus the async harness F7 that CI runs) are measured apart, then together.
# Writes a text report per level, an HTML report of the whole, and a summary table on stdout
# and, on GitHub Actions, in the job summary. A failing test fails the script.
#
# LLVM_PROFDATA and LLVM_COV name the LLVM tools that match the compiler (llvm-profdata-20 and
# llvm-cov-20 for clang-20); by default llvm-profdata and llvm-cov, through xcrun on macOS.
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <build directory> <output directory>" >&2
    exit 2
fi
build=$(cd "$1" && pwd)
mkdir -p "$2"
out=$(cd "$2" && pwd)
source_dir=$(cd "$(dirname "$0")/../.." && pwd)

tool_prefix=
if [ "$(uname -s)" = Darwin ]; then
    tool_prefix="xcrun "
fi
profdata=${LLVM_PROFDATA:-${tool_prefix}llvm-profdata}
cov=${LLVM_COV:-${tool_prefix}llvm-cov}

# ctrace, then every test executable: each holds part of the code the tests ran.
objects=
for binary in "$build"/ctrace_*; do
    if [ -f "$binary" ] && [ -x "$binary" ]; then
        objects="$objects -object $binary"
    fi
done

rm -rf "$out/profiles" "$out/html"
mkdir -p "$out/profiles/unit" "$out/profiles/integration"

# %4m: one profile per binary, merged by the processes that share it. Tests that run ctrace
# with an emptied environment (env -i) lose the variable, and so their share of the coverage.
LLVM_PROFILE_FILE="$out/profiles/unit/%4m.profraw" \
    ctest --test-dir "$build" -L unit --output-on-failure
LLVM_PROFILE_FILE="$out/profiles/integration/%4m.profraw" \
    ctest --test-dir "$build" -L integration --output-on-failure
(cd "$source_dir" && LLVM_PROFILE_FILE="$out/profiles/integration/%4m.profraw" \
    python3 tests/BTP-STACK-ANALYZER-F7 --ctrace-bin "$build/ctrace" \
    --iterations 1 --warmups 0 --min-speedup-ratio 0 > "$out/f7.log")

$profdata merge -sparse "$out"/profiles/unit/*.profraw -o "$out/unit.profdata"
$profdata merge -sparse "$out"/profiles/integration/*.profraw -o "$out/integration.profdata"
$profdata merge -sparse "$out/unit.profdata" "$out/integration.profdata" -o "$out/all.profdata"

sources="$source_dir/src $source_dir/include $source_dir/main.cpp"
# llvm-cov warns that functions "have mismatched data": an inline function that one binary
# uses and another does not emit has two records. Each binary's counts are still read from its
# own record, so the totals are right.
for level in unit integration all; do
    # shellcheck disable=SC2086 # $objects and $sources are lists of words.
    $cov report "$build/ctrace" $objects -instr-profile "$out/$level.profdata" $sources \
        > "$out/$level.txt"
done
# shellcheck disable=SC2086
$cov show "$build/ctrace" $objects -instr-profile "$out/all.profdata" -format=html \
    -output-dir "$out/html" $sources

# The TOTAL row of `llvm-cov report`: regions, missed, cover, functions, missed, executed,
# lines, missed, cover, branches, missed, cover.
{
    echo "| Tests | Lines | Functions | Branches |"
    echo "|---|---|---|---|"
    for level in unit integration all; do
        awk -v level="$level" \
            '$1 == "TOTAL" { printf "| %s | %s | %s | %s |\n", level, $10, $7, $13 }' \
            "$out/$level.txt"
    done
} > "$out/summary.md"
cat "$out/summary.md"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "### Coverage of src/, include/ and main.cpp"
        echo
        cat "$out/summary.md"
    } >> "$GITHUB_STEP_SUMMARY"
fi
