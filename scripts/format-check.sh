#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

# Collect only first-party source files from this repository.
files=()
first_party_paths=(
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/src"
    "${REPO_ROOT}/tests"
    "${REPO_ROOT}/main.cpp"
)
for path in "${first_party_paths[@]}"; do
    if [ -d "${path}" ]; then
        while IFS= read -r -d '' file; do
            files+=("${file}")
        done < <(find "${path}" -type f \
            \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' \) -print0)
    elif [ -f "${path}" ]; then
        case "${path}" in
            *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx)
                files+=("${path}")
                ;;
        esac
    fi
done

if [ "${#files[@]}" -eq 0 ]; then
    echo "No source files to check."
    exit 0
fi

if [ -n "${CLANG_FORMAT:-}" ]; then
    CF_BIN="${CLANG_FORMAT}"
elif command -v clang-format-20 >/dev/null 2>&1; then
    CF_BIN="clang-format-20"
elif command -v clang-format >/dev/null 2>&1; then
    CF_BIN="clang-format"
else
    CF_BIN="clang-format"
fi

if ! command -v "${CF_BIN}" >/dev/null 2>&1; then
    echo "clang-format binary not found: ${CF_BIN}"
    echo "Set CLANG_FORMAT or install clang-format."
    exit 1
fi

if [ "${CF_BIN}" = "clang-format" ]; then
    version_line="$(${CF_BIN} --version 2>/dev/null || true)"
    if [[ "${version_line}" =~ ([0-9]+)\. ]]; then
        major="${BASH_REMATCH[1]}"
        if [ "${major}" -lt 20 ]; then
            echo "clang-format version too old (${version_line}). Need >= 20."
            echo "Set CLANG_FORMAT to clang-format-20 or install a newer clang-format."
            exit 1
        fi
    fi
fi

echo "Using ${CF_BIN}."
echo "Checking formatting on ${#files[@]} first-party files..."
if ! "${CF_BIN}" --dry-run --Werror "${files[@]}"; then
    echo "Formatting check failed. Run scripts/format.sh to fix."
    exit 1
fi

echo "Formatting is clean."
