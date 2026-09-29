# SPDX-License-Identifier: Apache-2.0
#
# Matches the files of a release tree against the inventory of the third-party code it ships
# (third-party.txt). The first line whose glob matches a file applies to it; `*` also matches
# `/`. Arguments: the inventory, then a list of paths relative to the archive root. Prints
# tab-separated lines:
#   entry <line> <glob> [<license path>...]   for each line of the inventory, in order
#   file <line> <path>                        for each path; <line> is 0 when none matches

function to_regex(glob,    result, i, c) {
    result = "^"
    for (i = 1; i <= length(glob); i++) {
        c = substr(glob, i, 1)
        if (c == "*")
            result = result ".*"
        else if (c == "?")
            result = result "."
        else if (index("\\^$.[]|()+{}", c))
            result = result "\\" c
        else
            result = result c
    }
    return result "$"
}

FILENAME == ARGV[1] {
    if ($0 ~ /^[ \t]*(#|$)/)
        next
    count++
    pattern[count] = to_regex($1)
    out = "entry\t" count "\t" $1
    for (i = 2; i <= NF; i++)
        out = out "\t" $i
    print out
    next
}

{
    for (i = 1; i <= count; i++)
        if ($0 ~ pattern[i])
            break
    print "file\t" (i <= count ? i : 0) "\t" $0
}
