#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Stands in for a tool that runs long: starts a sleep of CTRACE_FAKE_SLEEP seconds (600 by
# default), writes its own pid and the sleep's to CTRACE_FAKE_PID_FILE, one per line, then waits
# for the sleep. It prints nothing, whatever its arguments.
sleep "${CTRACE_FAKE_SLEEP:-600}" &
printf '%s\n%s\n' "$$" "$!" > "$CTRACE_FAKE_PID_FILE.partial"
mv "$CTRACE_FAKE_PID_FILE.partial" "$CTRACE_FAKE_PID_FILE"
wait
