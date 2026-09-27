#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for coretrace-runtime-analyzer. It checks the command line ctrace must use:
#   --format sarif --timeout <seconds> -o <run directory>/<name> [args] -- [compiler args] <source>
# with the run directory existing and the source last, and exits 64 otherwise. Then it prints
# the SARIF the real tool prints for the fixtures, with the source spelled as it was given:
# a heap overflow for heap_overflow.c, a double free on line 12 for double_free.c, nothing for
# others. --fake-exit=<code> ends with that code instead (2: could not build or run),
# --fake-hang never ends. When CTRACE_FAKE_RUNTIME_LOG is set, the -o path is appended to it.
contract() { echo "fake-runtime-analyzer: $1" >&2; exit 64; }
[ "$1" = --format ] && [ "$2" = sarif ] || contract "expected --format sarif first"
[ "$3" = --timeout ] || contract "expected --timeout"
case "$4" in ''|*[!0-9]*) contract "--timeout needs whole seconds" ;; esac
[ "$5" = -o ] || contract "expected -o"
program="$6"
[ -d "$(dirname "$program")" ] || contract "the run directory does not exist: $program"
shift 6
mode=
while [ "$#" -gt 0 ] && [ "$1" != -- ]; do
    case "$1" in
        --fake-exit=*) mode="exit ${1#--fake-exit=}" ;;
        --fake-hang) mode=hang ;;
    esac
    shift
done
[ "$1" = -- ] || contract "expected -- before the compiler arguments"
for source; do :; done
[ -f "$source" ] || contract "the source comes last: $source"
if [ -n "${CTRACE_FAKE_RUNTIME_LOG:-}" ]; then
    echo "$program" >> "$CTRACE_FAKE_RUNTIME_LOG"
fi
case "$mode" in
    hang) while :; do sleep 1; done ;;
    exit*) echo "runtime-analyzer: $source: build failed" >&2; exit "${mode#exit }" ;;
esac

# The real tool reports compiler and linker warnings on stderr; ctrace must still read the log.
echo "ld: warning: object file was built for a newer version" >&2
case "$(basename "$source")" in
    heap_overflow.c) rule=heap-buffer-overflow cwe=CWE-122 line=7 column=15
        message="heap-buffer-overflow WRITE of size 4" ;;
    double_free.c) rule=double-free cwe=CWE-415 line=12 column=5 message="double free" ;;
    *) rule= ;;
esac
if [ -z "$rule" ]; then
    echo '{"version": "2.1.0", "runs": [{"tool": {"driver": {"name": "coretrace-runtime-analyzer"}}, "results": []}]}'
    exit 0
fi
cat <<SARIF
{
  "version": "2.1.0",
  "runs": [{
    "tool": {"driver": {"name": "coretrace-runtime-analyzer"}},
    "results": [{
      "ruleId": "$rule",
      "level": "error",
      "message": {"text": "$message"},
      "properties": {"cwe": "$cwe"},
      "locations": [{"physicalLocation": {
        "artifactLocation": {"uri": "$source"},
        "region": {"startLine": $line, "startColumn": $column}}}]
    }]
  }]
}
SARIF
exit 1
