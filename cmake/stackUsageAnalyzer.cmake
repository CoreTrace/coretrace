include(FetchContent)

FetchContent_Declare(
  stack_analyzer
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-stack-analyzer.git
  GIT_TAG v0.17.0
)

FetchContent_GetProperties(stack_analyzer)
if(NOT stack_analyzer_POPULATED)
    FetchContent_Populate(stack_analyzer)
    add_subdirectory(${stack_analyzer_SOURCE_DIR} ${stack_analyzer_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Copy upstream default models into config/models/ so that tool-config.json
# can reference them with paths relative to the config directory, without
# ever pointing into _deps/.
# Custom model files already present in config/models/ are not affected
# (they live at the root of the directory, upstream models are in subdirs).
file(COPY "${stack_analyzer_SOURCE_DIR}/models/"
     DESTINATION "${CMAKE_SOURCE_DIR}/config/models")
