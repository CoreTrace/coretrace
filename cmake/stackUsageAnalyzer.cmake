include(FetchContent)

FetchContent_Declare(
  stack_analyzer
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-stack-analyzer.git
  GIT_TAG main
)

FetchContent_MakeAvailable(stack_analyzer)
