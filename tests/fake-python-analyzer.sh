#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for coretrace-python-analyzer: prints the SARIF that tool writes for one file (the
# path relative to the file's directory, one finding suppressed in source) and exits with
# --fake-exit=<code>, 1 by default, which is how the real tool reports findings.
code=1
for arg in "$@"; do
    case "$arg" in
        --fake-exit=*) code="${arg#--fake-exit=}" ;;
        *) file="$arg" ;;
    esac
done
case "$file" in
    *.py) ;;
    *) echo "error: not a Python file: $file" >&2; exit 2 ;;
esac
name="${file##*/}"
# Shell builtins only: the relocation test runs this with an empty PATH.
while IFS= read -r line; do printf '%s\n' "$line"; done <<SARIF
{
  "version": "2.1.0",
  "runs": [
    {
      "originalUriBaseIds": {"SRCROOT": {"uri": "file:///somewhere/"}},
      "tool": {"driver": {"name": "coretrace-python-analyzer"}},
      "results": [
        {
          "ruleId": "dangerous-eval",
          "level": "error",
          "message": {"text": "call to python.builtins.eval executes dynamically built code"},
          "locations": [{"physicalLocation": {
            "artifactLocation": {"uri": "$name", "uriBaseId": "SRCROOT"},
            "region": {"startLine": 6, "startColumn": 12}}}]
        },
        {
          "ruleId": "dangerous-eval",
          "level": "error",
          "message": {"text": "call to python.builtins.eval executes dynamically built code"},
          "locations": [{"physicalLocation": {
            "artifactLocation": {"uri": "$name", "uriBaseId": "SRCROOT"},
            "region": {"startLine": 10, "startColumn": 12}}}],
          "suppressions": [{"kind": "inSource"}]
        }
      ]
    }
  ]
}
SARIF
exit "$code"
