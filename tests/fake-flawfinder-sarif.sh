#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for flawfinder --sarif: one finding on line 1 of the last file it is given, its rule
# described as flawfinder describes it, with a title and a CWE link but no long description.
for file; do :; done
cat <<SARIF
{"version": "2.1.0", "runs": [{"tool": {"driver": {"name": "Flawfinder", "rules": [
  {"id": "FF1001", "name": "buffer/strcpy",
   "shortDescription": {"text": "Does not check for buffer overflows when copying to destination (CWE-120)."},
   "helpUri": "https://cwe.mitre.org/data/definitions/120.html"}]}},
 "results": [{"ruleId": "FF1001", "level": "error",
   "message": {"text": "buffer/strcpy:Does not check for buffer overflows when copying to destination (CWE-120)."},
   "locations": [{"physicalLocation": {"artifactLocation": {"uri": "$file"}, "region": {"startLine": 1}}}]}]}]}
SARIF
