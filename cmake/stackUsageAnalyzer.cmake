# SPDX-License-Identifier: Apache-2.0
include(FetchContent)

set(CORETRACE_LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "Disable logger examples" FORCE)
set(CORETRACE_LOGGER_BUILD_TESTS OFF CACHE BOOL "Disable logger tests" FORCE)

function(_coretrace_patch_imported_link_property _target _property _stale_value _replacement_value)
  if(NOT TARGET "${_target}")
    return()
  endif()

  get_target_property(_current_value "${_target}" "${_property}")
  if(NOT _current_value OR _current_value STREQUAL "_current_value-NOTFOUND")
    return()
  endif()

  string(REPLACE "${_stale_value}" "${_replacement_value}" _updated_value "${_current_value}")
  if(NOT _updated_value STREQUAL _current_value)
    set_target_properties("${_target}" PROPERTIES
      ${_property} "${_updated_value}"
    )
  endif()
endfunction()

function(_coretrace_patch_llvm_diaguids _replacement_path)
  if(NOT WIN32 OR NOT TARGET LLVMDebugInfoPDB)
    return()
  endif()

  set(_coretrace_stale_diaguids_path
      "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/DIA SDK/lib/amd64/diaguids.lib")

  foreach(_property_name
          INTERFACE_LINK_LIBRARIES
          IMPORTED_LINK_INTERFACE_LIBRARIES
          IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG
          IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE
          IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO
          IMPORTED_LINK_INTERFACE_LIBRARIES_MINSIZEREL)
    _coretrace_patch_imported_link_property(
      LLVMDebugInfoPDB
      ${_property_name}
      "${_coretrace_stale_diaguids_path}"
      "${_replacement_path}"
    )
  endforeach()
endfunction()

function(_coretrace_patch_fetched_file _path _search _replacement)
  if(NOT EXISTS "${_path}")
    return()
  endif()

  file(READ "${_path}" _content)
  string(REPLACE "${_search}" "${_replacement}" _updated "${_content}")
  if(NOT _updated STREQUAL _content)
    file(WRITE "${_path}" "${_updated}")
  endif()
endfunction()

function(_coretrace_force_msvc_runtime target_name)
  if(MSVC AND TARGET "${target_name}")
    set_property(TARGET "${target_name}" PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
  endif()
endfunction()

FetchContent_Declare(
  coretrace-logger
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-log.git
  GIT_TAG main
  EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(coretrace-logger)
if(NOT coretrace-logger_POPULATED)
  FetchContent_Populate(coretrace-logger)
endif()

if(WIN32)
  file(COPY_FILE
    "${CMAKE_SOURCE_DIR}/src/ThirdParty/CoretraceLoggerWindows.cpp"
    "${coretrace-logger_SOURCE_DIR}/src/logger.cpp"
    ONLY_IF_DIFFERENT
  )
endif()

if(NOT TARGET coretrace_logger)
  add_subdirectory("${coretrace-logger_SOURCE_DIR}" "${coretrace-logger_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()
_coretrace_force_msvc_runtime(coretrace_logger)

FetchContent_Declare(
  cc
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-compiler.git
  GIT_TAG main
  EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(cc)
if(NOT cc_POPULATED)
  FetchContent_Populate(cc)
endif()

_coretrace_patch_fetched_file(
  "${cc_SOURCE_DIR}/CMakeLists.txt"
  [=[message(STATUS "Using Clang resource dir: ${CLANG_RESOURCE_DIR}")]=]
  [=[file(TO_CMAKE_PATH "${CLANG_RESOURCE_DIR}" CLANG_RESOURCE_DIR)
message(STATUS "Using Clang resource dir: ${CLANG_RESOURCE_DIR}")]=]
)
_coretrace_patch_fetched_file(
  "${cc_SOURCE_DIR}/CMakeLists.txt"
  "if(NOT LLVM_ENABLE_RTTI)\n  target_compile_options(compilerlib_static PRIVATE -fno-rtti)\nendif()"
  "if(NOT LLVM_ENABLE_RTTI)\n  if(MSVC)\n    target_compile_options(compilerlib_static PRIVATE /GR-)\n  else()\n    target_compile_options(compilerlib_static PRIVATE -fno-rtti)\n  endif()\nendif()"
)
_coretrace_patch_fetched_file(
  "${cc_SOURCE_DIR}/CMakeLists.txt"
  "if(NOT LLVM_ENABLE_RTTI)\n  target_compile_options(compilerlib_shared PRIVATE -fno-rtti)\nendif()"
  "if(NOT LLVM_ENABLE_RTTI)\n  if(MSVC)\n    target_compile_options(compilerlib_shared PRIVATE /GR-)\n  else()\n    target_compile_options(compilerlib_shared PRIVATE -fno-rtti)\n  endif()\nendif()"
)
_coretrace_patch_fetched_file(
  "${cc_SOURCE_DIR}/CMakeLists.txt"
  "if(NOT LLVM_ENABLE_RTTI)\n  target_compile_options(cc PRIVATE -fno-rtti)\nendif()"
  "if(NOT LLVM_ENABLE_RTTI)\n  if(MSVC)\n    target_compile_options(cc PRIVATE /GR-)\n  else()\n    target_compile_options(cc PRIVATE -fno-rtti)\n  endif()\nendif()"
)

if(WIN32)
  set(_coretrace_diaguids_candidates)
  file(GLOB_RECURSE _coretrace_diaguids_candidates
    LIST_DIRECTORIES false
    "C:/Program Files/Microsoft Visual Studio/*/*/DIA SDK/lib/amd64/diaguids.lib"
    "C:/Program Files (x86)/Microsoft Visual Studio/*/*/DIA SDK/lib/amd64/diaguids.lib"
  )

  if(_coretrace_diaguids_candidates)
    list(GET _coretrace_diaguids_candidates 0 _coretrace_diaguids_path)
  endif()

  # Import and patch LLVM/Clang targets before dependent subprojects link
  # against them. Some Windows LLVM packages ship a stale DIA SDK path in the
  # LLVMDebugInfoPDB imported target.
  find_package(LLVM REQUIRED CONFIG)
  find_package(Clang REQUIRED CONFIG)

  if(_coretrace_diaguids_path)
    _coretrace_patch_llvm_diaguids("${_coretrace_diaguids_path}")
  endif()

  set(_coretrace_cc_windows_runtime_dir
      "${CMAKE_SOURCE_DIR}/src/ThirdParty/coretrace-compiler/windows")

  foreach(_runtime_file
          ct_runtime_internal.h
          ct_runtime_helpers.h
          ct_runtime_alloc.cpp
          ct_runtime_backtrace.cpp
          ct_runtime_env.cpp
          ct_runtime_vtable.cpp)
    file(COPY_FILE
      "${_coretrace_cc_windows_runtime_dir}/${_runtime_file}"
      "${cc_SOURCE_DIR}/src/runtime/${_runtime_file}"
      ONLY_IF_DIFFERENT
    )
  endforeach()
endif()

if(NOT TARGET compilerlib_static)
  add_subdirectory("${cc_SOURCE_DIR}" "${cc_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()

set(_coretrace_cc_extra_llvm_libs
  LLVMAArch64CodeGen
  LLVMAArch64AsmParser
  LLVMAArch64Desc
  LLVMAArch64Info
  LLVMAArch64Utils
  LLVMARMCodeGen
  LLVMARMAsmParser
  LLVMARMDesc
  LLVMARMInfo
  LLVMARMUtils
  LLVMBPFCodeGen
  LLVMBPFAsmParser
  LLVMBPFDesc
  LLVMBPFInfo
  LLVMWebAssemblyCodeGen
  LLVMWebAssemblyAsmParser
  LLVMWebAssemblyDesc
  LLVMWebAssemblyInfo
  LLVMWebAssemblyUtils
  LLVMRISCVCodeGen
  LLVMRISCVAsmParser
  LLVMRISCVDesc
  LLVMRISCVInfo
  LLVMRISCVTargetMCA
  LLVMNVPTXCodeGen
  LLVMNVPTXDesc
  LLVMNVPTXInfo
)
if(TARGET compilerlib_static)
  target_link_libraries(compilerlib_static PUBLIC ${_coretrace_cc_extra_llvm_libs})
endif()
if(TARGET compilerlib_shared)
  target_link_libraries(compilerlib_shared PRIVATE ${_coretrace_cc_extra_llvm_libs})
endif()

_coretrace_force_msvc_runtime(ct_instrument_runtime)
_coretrace_force_msvc_runtime(compilerlib_static)
_coretrace_force_msvc_runtime(compilerlib_shared)
_coretrace_force_msvc_runtime(cc)

if(WIN32 AND _coretrace_diaguids_path)
  _coretrace_patch_llvm_diaguids("${_coretrace_diaguids_path}")
endif()

FetchContent_Declare(
  stack_analyzer
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-stack-analyzer.git
  GIT_TAG v0.18.1
  EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(stack_analyzer)
if(NOT stack_analyzer_POPULATED)
  FetchContent_Populate(stack_analyzer)
endif()

_coretrace_patch_fetched_file(
  "${stack_analyzer_SOURCE_DIR}/CMakeLists.txt"
  "set(LLVM_LINK_LLVM_DYLIB ON)"
  "if(WIN32)\n  set(LLVM_LINK_LLVM_DYLIB OFF)\nelse()\n  set(LLVM_LINK_LLVM_DYLIB ON)\nendif()"
)
_coretrace_patch_fetched_file(
  "${stack_analyzer_SOURCE_DIR}/CMakeLists.txt"
  "if(ENABLE_STACK_USAGE)\n    target_compile_options(stack_usage_analyzer_lib PRIVATE -fstack-usage)\nendif()"
  "if(ENABLE_STACK_USAGE AND NOT MSVC)\n    target_compile_options(stack_usage_analyzer_lib PRIVATE -fstack-usage)\nendif()"
)
_coretrace_patch_fetched_file(
  "${stack_analyzer_SOURCE_DIR}/CMakeLists.txt"
  "    if(ENABLE_STACK_USAGE)\n        target_compile_options(stack_usage_analyzer PRIVATE -fstack-usage)\n    endif()"
  "    if(ENABLE_STACK_USAGE AND NOT MSVC)\n        target_compile_options(stack_usage_analyzer PRIVATE -fstack-usage)\n    endif()"
)

if(WIN32)
  set(_coretrace_stack_windows_dir
      "${CMAKE_SOURCE_DIR}/src/ThirdParty/coretrace-stack-analyzer/windows")

  foreach(_stack_file
          mangle.hpp
          mangle.cpp)
    if(_stack_file STREQUAL "mangle.hpp")
      set(_stack_destination "${stack_analyzer_SOURCE_DIR}/include/${_stack_file}")
    else()
      set(_stack_destination "${stack_analyzer_SOURCE_DIR}/src/${_stack_file}")
    endif()

    file(COPY_FILE
      "${_coretrace_stack_windows_dir}/${_stack_file}"
      "${_stack_destination}"
      ONLY_IF_DIFFERENT
    )
  endforeach()
endif()

if(NOT TARGET stack_usage_analyzer_lib)
  add_subdirectory("${stack_analyzer_SOURCE_DIR}" "${stack_analyzer_BINARY_DIR}" EXCLUDE_FROM_ALL)
endif()
_coretrace_force_msvc_runtime(stack_usage_analyzer_lib)
_coretrace_force_msvc_runtime(stack_usage_analyzer)

# Copy upstream default models into config/models/ so that tool-config.json
# can reference them with paths relative to the config directory, without
# ever pointing into _deps/.
# Custom model files already present in config/models/ are not affected
# (they live at the root of the directory, upstream models are in subdirs).
file(COPY "${stack_analyzer_SOURCE_DIR}/models/"
     DESTINATION "${CMAKE_SOURCE_DIR}/config/models")
