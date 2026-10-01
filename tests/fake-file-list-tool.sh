#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for cppcheck or tscancode run on a --file-list: reports one finding on line 1 of
# every file the list names, in cppcheck's template when given --template=, in tscancode's
# format otherwise. Without --file-list it exits 64.
list=
cppcheck=
for arg in "$@"; do
    case "$arg" in
        --file-list=*) list="${arg#--file-list=}" ;;
        --template=*) cppcheck=1 ;;
    esac
done
[ -n "$list" ] && [ -f "$list" ] || { echo "fake-file-list-tool: no --file-list" >&2; exit 64; }
while IFS= read -r file; do
    if [ -n "$cppcheck" ]; then
        echo "$file:1:1: warning: listed [listed] [CWE-0]"
    else
        echo "[$file:1]: (warning) listed"
    fi
done < "$list"
