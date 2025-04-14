include(ExternalProject)

# Create scripts directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/scripts)

ExternalProject_Add(
    tscancode
    GIT_REPOSITORY https://github.com/Tencent/TscanCode.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/tscancode
    PATCH_COMMAND
        # Apply patch to fix the MYSTACKSIZE issue
        chmod +x ${CMAKE_SOURCE_DIR}/scripts/fix_tscancode.sh && 
        ${CMAKE_SOURCE_DIR}/scripts/fix_tscancode.sh ${CMAKE_BINARY_DIR}/tscancode/src/tscancode/trunk
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        COMMAND cd ${CMAKE_BINARY_DIR}/tscancode/src/tscancode/trunk && make
    INSTALL_COMMAND ""
)

# Add a custom target for running TScanCode analysis
add_custom_target(
    run_tscancode
    COMMAND ${CMAKE_BINARY_DIR}/tscancode/src/tscancode/trunk/tscancode --enable=all ${CMAKE_SOURCE_DIR}/src
    DEPENDS tscancode
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running TScanCode analysis..."
)
