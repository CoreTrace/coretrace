#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for coretrace-python-analyzer in project mode, on tests/python/project: it takes
# the project directory, prints the SARIF the real tool writes for that project (paths
# relative to the directory, a cross-module finding in app/main.py, one in app/other.py, one
# suppressed in source) and exits with --fake-exit=<code>, 1 by default, which is how the
# real tool reports findings.
code=1
for arg in "$@"; do
    case "$arg" in
        --fake-exit=*) code="${arg#--fake-exit=}" ;;
        *) target="$arg" ;;
    esac
done
if [ ! -d "$target" ]; then
    echo "error: expected a project directory, got: $target" >&2
    exit 2
fi
# Shell builtins only: the relocation test runs this with an empty PATH.
while IFS= read -r line; do printf '%s\n' "$line"; done <<'SARIF'
{
  "version": "2.1.0",
  "runs": [
    {
      "tool": {"driver": {"name": "coretrace-python-analyzer"}},
      "results": [
        {
          "ruleId": "command-injection",
          "level": "error",
          "message": {"text": "Command injection: stdin input reaches python.os.system through app.helpers.execute"},
          "locations": [{"physicalLocation": {
            "artifactLocation": {"uri": "app/main.py", "uriBaseId": "SRCROOT"},
            "region": {"startLine": 7, "startColumn": 5}}}]
        },
        {
          "ruleId": "dangerous-eval",
          "level": "error",
          "message": {"text": "call to python.builtins.eval executes dynamically built code"},
          "locations": [{"physicalLocation": {
            "artifactLocation": {"uri": "app/other.py", "uriBaseId": "SRCROOT"},
            "region": {"startLine": 4, "startColumn": 12}}}]
        },
        {
          "ruleId": "dangerous-eval",
          "level": "error",
          "message": {"text": "call to python.builtins.eval executes dynamically built code"},
          "locations": [{"physicalLocation": {
            "artifactLocation": {"uri": "app/main.py", "uriBaseId": "SRCROOT"},
            "region": {"startLine": 11, "startColumn": 12}}}],
          "suppressions": [{"kind": "inSource"}]
        }
      ]
    }
  ]
}
SARIF
exit "$code"
