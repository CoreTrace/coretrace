#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Compiles a coretrace-python-analyzer checkout into a standalone executable with Nuitka: the
# Python runtime, the standard library and the analyzer are compiled or copied into
# <output dir>/coretrace_python.dist, which runs without Python. The bundled plugins are loaded
# from their .py files at run time, so they are copied as is next to the compiled package. The
# analyzer's license and notice go to its licenses/ directory, with Nuitka's: the executable
# holds Nuitka's runtime library.
#
# The executable needs a glibc at least as old as the build machine's: the release builds it on
# the oldest distribution it supports. CMake (ENABLE_PYTHON_ANALYZER) and the release image both
# call this script, so the flags live in one place.
#
# Usage: PYTHON=<python with nuitka> build-python-analyzer.sh <analyzer checkout> <output dir>
set -eu

source_dir=$1
output_dir=$2

"${PYTHON:-python3}" -m nuitka \
    --mode=standalone \
    --assume-yes-for-downloads \
    --quiet \
    --output-dir="$output_dir" \
    --output-filename=coretrace-python-analyzer \
    --include-package=coretrace_python \
    --include-raw-dir="$source_dir/src/coretrace_python/bundled=coretrace_python/bundled" \
    "$source_dir/src/coretrace_python"

licenses="$output_dir/coretrace_python.dist/licenses"
mkdir -p "$licenses/coretrace-python-analyzer" "$licenses/nuitka"
cp "$source_dir/LICENSE" "$source_dir/NOTICE" "$licenses/coretrace-python-analyzer/"
"${PYTHON:-python3}" - "$licenses/nuitka" <<'EOF'
import importlib.metadata
import shutil
import sys

nuitka = importlib.metadata.distribution("nuitka")
files = [f for f in nuitka.files or [] if f.parts[0].endswith(".dist-info") and "licenses" in f.parts]
if not files:
    sys.exit("Nuitka's installation carries no license file")
for file in files:
    shutil.copy(nuitka.locate_file(file), sys.argv[1])
EOF
