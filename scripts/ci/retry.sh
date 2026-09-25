# SPDX-License-Identifier: Apache-2.0
#
# retry <attempts> <first delay in seconds> <command> [args...]
#
# Runs the command until it succeeds, at most <attempts> times, waiting <first delay> seconds
# after the first failure and twice as long after each next one. Returns the command's last
# status. Meant to be sourced by CI scripts whose steps depend on the network: a mirror that
# drops connections for a minute must not fail a release.
retry() {
    retry_attempts=$1
    retry_delay=$2
    shift 2
    retry_run=1
    while :; do
        "$@" && return 0
        retry_status=$?
        if [ "$retry_run" -ge "$retry_attempts" ]; then
            echo "retry: '$*' failed ${retry_run} time(s), giving up" >&2
            return "$retry_status"
        fi
        echo "retry: '$*' failed (attempt ${retry_run}/${retry_attempts}, status ${retry_status}); retrying in ${retry_delay}s" >&2
        sleep "$retry_delay"
        retry_run=$((retry_run + 1))
        retry_delay=$((retry_delay * 2))
    done
}
