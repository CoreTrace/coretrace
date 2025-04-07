include(ExternalProject)

# GMP
find_library(GMP_LIBRARY NAMES gmp)
find_path(GMP_INCLUDE_DIR NAMES gmp.h)

if(NOT GMP_LIBRARY OR NOT GMP_INCLUDE_DIR)
  if(IS_LINUX AND AUTO_INSTALL_DEPENDENCIES)
    message(STATUS "GMP not found. It will be installed by the dependency script if AUTO_INSTALL_DEPENDENCIES is enabled.")
  else  
    if(IS_MACOS)
      message(WARNING "GMP not found. Install it with 'brew install gmp' on macOS. IKOS analysis will be disabled.")
    elseif(IS_LINUX)
      message(WARNING "GMP not found. Install it with 'apt-get install libgmp-dev' on Linux or run CMake with -DAUTO_INSTALL_DEPENDENCIES=ON. IKOS analysis will be disabled.")
    else()
      message(WARNING "GMP not found. IKOS analysis will be disabled.")
    endif()
    return()
  endif()
endif()

# Boost - use same approach as main CMakeLists to ensure compatibility
if(NOT Boost_FOUND)
  # Set policy for Boost detection if not already set in main CMakeLists
  if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
  endif()
  
  # First try with required components
  find_package(Boost COMPONENTS system filesystem)
  
  if(NOT Boost_FOUND)
    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(BOOST QUIET boost_system boost_filesystem)
      if(BOOST_FOUND)
        set(Boost_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS})
        set(Boost_LIBRARIES ${BOOST_LIBRARIES})
        set(Boost_FOUND TRUE)
      endif()
    endif()
  endif()
  
  if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Install it with 'brew install boost' on macOS or 'apt-get install libboost-all-dev' on Linux.")
  endif()
endif()

# TBB
find_library(TBB_LIBRARY NAMES tbb)
if(NOT TBB_LIBRARY)
  message(FATAL_ERROR "TBB not found. Install it with 'brew install tbb' on macOS or 'apt-get install libtbb-dev' on Linux.")
endif()

# LLVM 14
find_program(LLVM_CONFIG_EXECUTABLE llvm-config HINTS "/opt/llvm-14/bin" "/opt/homebrew/opt/llvm@14/bin")
if(NOT LLVM_CONFIG_EXECUTABLE)
  message(FATAL_ERROR "LLVM 14 not found. Install it with 'brew install llvm@14' on macOS or manually compile it for Linux.")
endif()

# SQLite
find_library(SQLITE_LIBRARY NAMES sqlite3)
if(NOT SQLITE_LIBRARY)
  message(FATAL_ERROR "SQLite not found. Install it with 'brew install sqlite' on macOS or 'apt-get install libsqlite3-dev' on Linux.")
endif()

# Apron (approximation, peut nécessiter un module personnalisé)
find_library(APRON_LIBRARY NAMES apron)
if(NOT APRON_LIBRARY)
  message(FATAL_ERROR "Apron not found. Install it with 'brew install apron' on macOS or 'apt-get install libapron-dev' on Linux.")
endif()

# Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
if(NOT Python3_FOUND)
  message(FATAL_ERROR "Python3 not found. Ensure python3 is installed.")
endif()

# Configuration pour macOS (si cross-compilation)
if(CMAKE_CROSSCOMPILING)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isysroot /usr/local/macos-sdk -target arm64-apple-darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot /usr/local/macos-sdk -target arm64-apple-darwin")
endif()

ExternalProject_Add(
    ikos
    GIT_REPOSITORY https://github.com/NASA-SW-VnV/ikos.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/ikos
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/ikos/src/ikos mkdir -p build &&
        ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/ikos/src/ikos/build cmake
            -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/ikos/src/ikos-build
            -DCMAKE_BUILD_TYPE=Debug
            -DLLVM_CONFIG_EXECUTABLE=${LLVM_CONFIG_EXECUTABLE}
            -DPYTHON_EXECUTABLE:FILEPATH=${Python3_EXECUTABLE}
            -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS}"
            -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}"
            .. &&
        ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/ikos/src/ikos/build make -j4 &&
        ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/ikos/src/ikos/build make install
    INSTALL_COMMAND ""
)

# IKOS static analysis configuration
# This file configures the IKOS static analyzer for use with the project

# Find IKOS
find_program(IKOS_EXECUTABLE ikos)

# Add a custom target for running IKOS analysis
add_custom_target(ikos_analysis
    COMMENT "Running IKOS static analysis..."
)

# Add a function to add files to IKOS analysis
function(add_ikos_analysis target)
    if(IKOS_EXECUTABLE)
        add_custom_command(
            TARGET ikos_analysis
            COMMAND ${IKOS_EXECUTABLE} -a=boa ${CMAKE_CURRENT_BINARY_DIR}/${target}
            COMMENT "Running IKOS on ${target}"
        )
    else()
        message(STATUS "IKOS not found, skipping IKOS analysis for ${target}")
    endif()
endfunction()

# Add project executable to IKOS analysis
if(TARGET ctrace)
    add_ikos_analysis(ctrace)
endif()
