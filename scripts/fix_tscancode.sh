#!/bin/bash

set -e

TSCANCODE_SRC_DIR="$1"

if [ -z "$TSCANCODE_SRC_DIR" ]; then
  echo "Usage: $0 <tscancode_source_directory>"
  exit 1
fi

# Check if the directory exists
if [ ! -d "$TSCANCODE_SRC_DIR" ]; then
  echo "TscanCode source directory not found: $TSCANCODE_SRC_DIR"
  exit 1
fi

# Define the file to patch
TSCEXECUTOR_FILE="$TSCANCODE_SRC_DIR/cli/tscexecutor.cpp"

# Check if the file exists
if [ ! -f "$TSCEXECUTOR_FILE" ]; then
  echo "TscanCode executor file not found: $TSCEXECUTOR_FILE"
  exit 1
fi

echo "Patching TscanCode source..."

# Fix MYSTACKSIZE issue - replace with a constant expression
sed -i 's/#define MYSTACKSIZE.*/#define MYSTACKSIZE (1024 * 1024)/g' "$TSCEXECUTOR_FILE"

# Also check and fix any undefined MYSTACKSIZE by adding the define if it doesn't exist
if ! grep -q "#define MYSTACKSIZE" "$TSCEXECUTOR_FILE"; then
  # Add the definition near the top of the file after the includes
  sed -i '/#include/!b;:a;n;/^$/!ba;i\
#define MYSTACKSIZE (1024 * 1024)  // 1MB stack size
' "$TSCEXECUTOR_FILE"
fi

echo "TscanCode patched successfully"
exit 0
