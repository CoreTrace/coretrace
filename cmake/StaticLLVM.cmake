# SPDX-License-Identifier: Apache-2.0
include_guard(GLOBAL)

# The pinned analyzer forces LLVM_LINK_LLVM_DYLIB=ON, and some SDKs export
# static Clang targets whose interfaces still reference the LLVM dylib.
# Adapt target edges, not downloaded source files or SDK files. The default
# build never loads this module. Non-LLVM dependencies are preserved.
function(coretrace_replace_llvm_edges target)
    if(NOT TARGET "${target}")
        return()
    endif()
    get_target_property(aliased "${target}" ALIASED_TARGET)
    if(aliased)
        set(target "${aliased}")
    endif()
    get_property(visited GLOBAL PROPERTY CORETRACE_STATIC_LLVM_VISITED)
    if("${target}" IN_LIST visited)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY CORETRACE_STATIC_LLVM_VISITED "${target}")

    get_target_property(imported "${target}" IMPORTED)
    get_target_property(kind "${target}" TYPE)
    if(imported AND kind STREQUAL "STATIC_LIBRARY")
        get_target_property(archive "${target}" LOCATION)
        if(NOT EXISTS "${archive}")
            message(FATAL_ERROR
                "Incomplete static SDK: ${target} references missing archive '${archive}'. "
                "Install the SDK's static development packages or build the private SDK.")
        endif()
    endif()

    foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(dependencies "${target}" "${property}")
        if(NOT dependencies)
            continue()
        endif()
        set(updated)
        foreach(dependency IN LISTS dependencies)
            if(dependency STREQUAL "LLVM")
                list(APPEND updated coretrace_static_llvm)
            elseif(dependency STREQUAL "clang-cpp")
                list(APPEND updated coretrace_static_clang)
            else()
                list(APPEND updated "${dependency}")
                coretrace_replace_llvm_edges("${dependency}")
            endif()
        endforeach()
        set_property(TARGET "${target}" PROPERTY "${property}" "${updated}")
    endforeach()
endfunction()

function(coretrace_link_static_llvm analyzer)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
        message(FATAL_ERROR "CORETRACE_STATIC_LLVM currently supports Linux and macOS only")
    endif()

    # SDKs built against libLLVM omit Clang's individual LLVM edges. Restore
    # the frontend/driver/codegen and analyzer components explicitly. Do not
    # use "all": installed SDKs need not export LLVM_COMPONENT_LIBS.
    # The compiler calls LLVMInitializeAllTargets, so retain every configured
    # backend. Normal archive extraction is used, never whole-archive.
    llvm_map_components_to_libnames(llvm_archives
        Analysis BitReader BitWriter Core Coverage Extensions FrontendDriver
        FrontendHLSL FrontendOpenACC FrontendOpenMP FrontendOffloading IRReader
        Linker LTO MC Option Passes ProfileData Support Target WindowsDriver
        AllTargetsInfos AllTargetsCodeGens AllTargetsAsmParsers
        AllTargetsDescs AllTargetsMCAs)
    set(clang_archives
        clangAST clangBasic clangCodeGen clangDriver clangFrontend
        clangLex clangParse clangSema)
    foreach(library IN LISTS llvm_archives clang_archives)
        if(NOT TARGET "${library}")
            message(FATAL_ERROR "Static LLVM/Clang SDK is missing target ${library}")
        endif()
        get_target_property(kind "${library}" TYPE)
        if(NOT kind STREQUAL "STATIC_LIBRARY")
            message(FATAL_ERROR "Static LLVM/Clang SDK required: ${library} is ${kind}")
        endif()
    endforeach()

    add_library(coretrace_static_llvm INTERFACE)
    target_link_libraries(coretrace_static_llvm INTERFACE ${llvm_archives})
    add_library(coretrace_static_clang INTERFACE)
    target_link_libraries(coretrace_static_clang INTERFACE ${clang_archives})

    set_property(GLOBAL PROPERTY CORETRACE_STATIC_LLVM_VISITED
        coretrace_static_llvm coretrace_static_clang)
    foreach(root IN ITEMS "${analyzer}" ${clang_archives} ${llvm_archives})
        coretrace_replace_llvm_edges("${root}")
    endforeach()
    message(STATUS "CoreTrace static LLVM/Clang linkage: ${LLVM_PACKAGE_VERSION}")
endfunction()
