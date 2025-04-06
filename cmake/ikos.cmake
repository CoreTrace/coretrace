include(ExternalProject)

# GMP
find_library(GMP_LIBRARY NAMES gmp)
if(NOT GMP_LIBRARY)
  message(FATAL_ERROR "GMP not found. Install it with 'brew install gmp' on macOS or 'apt-get install libgmp-dev' on Linux.")
endif()

# Boost
find_package(Boost 1.65 REQUIRED COMPONENTS system filesystem)
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found. Install it with 'brew install boost' on macOS or 'apt-get install libboost-all-dev' on Linux.")
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
