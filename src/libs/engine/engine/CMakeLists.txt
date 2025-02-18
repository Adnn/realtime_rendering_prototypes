# TODO rename to "renderer" when the low-level lib stop using it as a name.
set(TARGET_NAME engine)

set(${TARGET_NAME}_HEADERS
    IntrospectProgram.h
    Lights.h
    Resources.h
    Semantic.h
    SemanticValues.h
    ShaderConstants.h
    ShaderProgramManager.h

    files/Loader.h

    log/Logging.h
    log/Logging-channels.h
    log/Logging-init.h
)

set(${TARGET_NAME}_SOURCES
    IntrospectProgram.cpp
    Resources.cpp
    ShaderConstants.cpp
    ShaderProgramManager.cpp

    files/Loader.cpp

    log/Logging-init.cpp
)

set(${TARGET_NAME}_INFRA
    CMakeFinds.cmake.in
)

file(GLOB_RECURSE ${TARGET_NAME}_RESOURCES "resources/*")


source_group(/ FILES ${${TARGET_NAME}_INFRA})

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}
             FILES
                ${${TARGET_NAME}_HEADERS}
                ${${TARGET_NAME}_SOURCES}
                ${${TARGET_NAME}_RESOURCES}
)

add_library(${TARGET_NAME}
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
    ${${TARGET_NAME}_INFRA}
    ${${TARGET_NAME}_RESOURCES}
)

add_library(ad::${TARGET_NAME} ALIAS ${TARGET_NAME})


cmc_target_current_include_directory(${TARGET_NAME})

cmc_cpp_all_warnings_as_errors(${TARGET_NAME} ENABLED ${BUILD_CONF_WarningAsError})

cmc_cpp_sanitizer(${TARGET_NAME} ${BUILD_CONF_Sanitizer})


##
## Dependencies
##

cmc_find_dependencies(FILE CMakeFinds.cmake.in)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        ad::math
        ad::reflect
        ad::renderer
        ad::platform
        ad::resource

    PRIVATE
        spdlog::spdlog
)


##
## Install
##

install(TARGETS ${TARGET_NAME}
        EXPORT ${TARGET_NAME}Targets)

include(cmc-install)
# Install the header files
cmc_install_with_folders(DESTINATION include/${TARGET_NAME}/${TARGET_NAME}
                         FILES ${${TARGET_NAME}_HEADERS})
# Prepare CMake package in both build and install trees
cmc_install_packageconfig(${TARGET_NAME} ${TARGET_NAME}Targets ${PROJECT_NAME}
                          FIND_FILE CMakeFinds.cmake.in
                          NAMESPACE ad::)
