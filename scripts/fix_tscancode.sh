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

# Define the files to patch
TSCEXECUTOR_FILE="$TSCANCODE_SRC_DIR/cli/tscexecutor.cpp"
TSCTHREADEXECUTOR_FILE="$TSCANCODE_SRC_DIR/cli/tscthreadexecutor.cpp"

# Check if the files exist
if [ ! -f "$TSCEXECUTOR_FILE" ]; then
  echo "TscanCode executor file not found: $TSCEXECUTOR_FILE"
  exit 1
fi

if [ ! -f "$TSCTHREADEXECUTOR_FILE" ]; then
  echo "TscanCode thread executor file not found: $TSCTHREADEXECUTOR_FILE"
  exit 1
fi

echo "Patching TscanCode source..."

# ---- Fix MYSTACKSIZE issue in tscexecutor.cpp ----
# Create a backup of the original file
cp "$TSCEXECUTOR_FILE" "${TSCEXECUTOR_FILE}.bak"

# Create a temporary file
TEMP_FILE=$(mktemp)

# Write our patch to the temporary file
cat > "$TEMP_FILE" << 'EOF'
/* Added by fix_tscancode.sh */
#include <limits.h>
#include <stdint.h>
#include <signal.h>

// Define MYSTACKSIZE as a compile-time constant
#define MYSTACKSIZE (1024 * 1024)  // 1MB stack size

// Declare mytstack at global scope to ensure it's available
static char mytstack[MYSTACKSIZE];
EOF

# Append the original file content, skipping any existing definitions of MYSTACKSIZE
grep -v "static.*MYSTACKSIZE\|static char mytstack" "$TSCEXECUTOR_FILE" >> "$TEMP_FILE"

# Replace the original file with our modified version
mv "$TEMP_FILE" "$TSCEXECUTOR_FILE"

# ---- Fix intptr_t issue in tscthreadexecutor.cpp ----
# Create a backup of the original file
cp "$TSCTHREADEXECUTOR_FILE" "${TSCTHREADEXECUTOR_FILE}.bak"

# Create a temporary file
TEMP_FILE=$(mktemp)

# Add <cstdint> after the last include
awk '
BEGIN { printed = 0 }
/#include/ { lastinc = NR }
{ print $0 }
NR == lastinc && !printed { 
  print "\n#include <cstdint>  /* Added by fix_tscancode.sh */"; 
  printed = 1;
}' "$TSCTHREADEXECUTOR_FILE" > "$TEMP_FILE"

# If no includes were found, add it at the top
if ! grep -q "#include <cstdint>" "$TEMP_FILE"; then
  ANOTHER_TEMP=$(mktemp)
  echo '#include <cstdint>  /* Added by fix_tscancode.sh */' > "$ANOTHER_TEMP"
  cat "$TEMP_FILE" >> "$ANOTHER_TEMP"
  mv "$ANOTHER_TEMP" "$TEMP_FILE"
  echo "Added <cstdint> include to tscthreadexecutor.cpp"
fi

# Replace the original file with our modified version
mv "$TEMP_FILE" "$TSCTHREADEXECUTOR_FILE"

echo "TscanCode patched successfully"
exit 0
