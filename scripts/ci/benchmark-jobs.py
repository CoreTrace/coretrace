#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Times one ctrace analysis at several -j values: wall-clock time and peak RSS of ctrace.

Prints a Markdown table, for a CI job summary. Informational: a slower run fails nothing,
only a run that does not complete (exit code other than 0, 2 or 3) does.

  benchmark-jobs.py --ctrace build/ctrace --jobs 1 --jobs 4 -- --static --compile-commands db
"""

from __future__ import annotations

import argparse
import os
import resource
import subprocess
import sys
import time

# 0 clean, 2 findings at or above --fail-on, 3 a tool could not run (one not installed).
COMPLETED_EXIT_CODES = (0, 2, 3)


def peak_rss_mib(usage: resource.struct_rusage) -> float:
    # ru_maxrss is in kilobytes on Linux and in bytes on macOS.
    scale = 1 if sys.platform == "darwin" else 1024
    return usage.ru_maxrss * scale / (1024 * 1024)


def run(ctrace: str, jobs: int, args: list[str]) -> tuple[float, float, int]:
    command = [ctrace, "--jobs", str(jobs), *args]
    start = time.monotonic()
    with open(os.devnull, "wb") as devnull:
        process = subprocess.Popen(command, stdout=devnull, stderr=devnull)
        _, status, usage = os.wait4(process.pid, 0)
    elapsed = time.monotonic() - start
    return elapsed, peak_rss_mib(usage), os.waitstatus_to_exitcode(status)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ctrace", required=True, help="the ctrace binary")
    parser.add_argument("--jobs", type=int, action="append", required=True,
                        help="a -j value to time; repeat for each")
    parser.add_argument("args", nargs=argparse.REMAINDER,
                        help="ctrace arguments, after --")
    options = parser.parse_args()
    args = options.args[1:] if options.args[:1] == ["--"] else options.args

    rows = []
    for jobs in options.jobs:
        elapsed, rss, code = run(options.ctrace, jobs, args)
        if code not in COMPLETED_EXIT_CODES:
            print(f"ctrace -j{jobs} did not complete (exit code {code})", file=sys.stderr)
            return 1
        rows.append((jobs, elapsed, rss, code))

    baseline = rows[0][1]
    print("| -j | wall-clock (s) | speed-up | ctrace peak RSS (MiB) | exit code |")
    print("|---:|---:|---:|---:|---:|")
    for jobs, elapsed, rss, code in rows:
        print(f"| {jobs} | {elapsed:.1f} | {baseline / elapsed:.2f}x | {rss:.0f} | {code} |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
