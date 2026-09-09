# SPDX-License-Identifier: Apache-2.0
include(FetchContent)

set(CORETRACE_COMPILER_GIT_TAG "main" CACHE STRING
  "Git ref used when fetching coretrace-compiler")
set(CORETRACE_STACK_ANALYZER_GIT_TAG "main" CACHE STRING
  "Git ref used when fetching coretrace-stack-analyzer")

FetchContent_Declare(
  cc
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-compiler.git
  GIT_TAG ${CORETRACE_COMPILER_GIT_TAG}
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(cc)

FetchContent_Declare(
  stack_analyzer
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-stack-analyzer.git
  GIT_TAG ${CORETRACE_STACK_ANALYZER_GIT_TAG}
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(stack_analyzer)

# Copy upstream default models into config/models/ so that tool-config.json
# can reference them with paths relative to the config directory, without
# ever pointing into _deps/.
file(COPY "${stack_analyzer_SOURCE_DIR}/models/"
     DESTINATION "${CMAKE_SOURCE_DIR}/config/models")
