#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for cppcheck or tscancode run on a --file-list: reports one finding on line 1 of
# every file the list names, in cppcheck's template when given --template=, in tscancode's
# format otherwise. A file named slow.c takes it 1.5 s. With --fake-latin1, the cppcheck message
# holds a byte that is not UTF-8 (0xe9, an accented e in Latin-1). With --fake-other-format, each
# finding comes in the format of the other tool, which this one's parser cannot read. With
# --fake-mixed, each file gets an information in this tool's format, then an error on line 2 in
# the other's. With --fake-header, it prints only an error in a header, in neither format. With
# --fake-chatter, it prints a configuration warning and no finding. Without --file-list it exits 64.
list=
cppcheck=0
other=0
mixed=
header=
chatter=
message=listed
for arg in "$@"; do
    case "$arg" in
        --file-list=*) list="${arg#--file-list=}" ;;
        --template=*) cppcheck=1 ;;
        --fake-latin1) message=$(printf 'caf\351') ;;
        --fake-other-format) other=1 ;;
        --fake-mixed) mixed=1 ;;
        --fake-header) header=1 ;;
        --fake-chatter) chatter=1 ;;
    esac
done
[ -n "$list" ] && [ -f "$list" ] || { echo "fake-file-list-tool: no --file-list" >&2; exit 64; }
if [ -n "$chatter" ]; then
    echo "fake-file-list-tool: Found unknown elements in configuration file 'std.cfg'"
    exit 0
fi
if [ -n "$header" ]; then
    echo "include/bad.h:1: error: Null pointer dereference: p"
    exit 0
fi
while IFS= read -r file; do
    [ "$(basename "$file")" = slow.c ] && sleep 1.5
    if [ -n "$mixed" ]; then
        if [ "$cppcheck" = 1 ]; then
            echo "$file:1:1: information: listed [listed] [CWE-0]"
            echo "[$file:2]: (error) hidden"
        else
            echo "[$file:1]: (information) listed"
            echo "$file:2:1: error: hidden [hidden] [CWE-0]"
        fi
        continue
    fi
    if [ "$cppcheck" != "$other" ]; then
        echo "$file:1:1: warning: $message [listed] [CWE-0]"
    else
        echo "[$file:1]: (warning) listed"
    fi
done < "$list"
