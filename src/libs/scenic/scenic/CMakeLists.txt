set(TARGET_NAME scenic)

set(${TARGET_NAME}_HEADERS
    AssimpUtils.h
    Camera.h
    ColorPalettes.h
    Hierarchy.h
    LoadScene.h
    Model.h
    Pose.h
    Shapes.h
    VertexStreamUtilities.h

    log/Logging.h
    log/Logging-channels.h
    log/Logging-init.h
)

set(${TARGET_NAME}_SOURCES
    Camera.cpp
    Hierarchy.cpp
    LoadScene.cpp
    Model.cpp
    Pose.cpp
    Shapes.cpp
    VertexStreamUtilities.cpp

    log/Logging-init.cpp
)

set(${TARGET_NAME}_INFRA
    CMakeFinds.cmake.in
)


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
        ad::engine
        ad::graphics
        ad::math
    
    PRIVATE
        assimp::assimp
)


##
## Install
##

install(TARGETS ${TARGET_NAME} EXPORT ${TARGET_NAME}Targets)

include(cmc-install)
# Install the header files
cmc_install_with_folders(DESTINATION include/${TARGET_NAME}/${TARGET_NAME}
                         FILES ${${TARGET_NAME}_HEADERS})
# Prepare CMake package in both build and install trees
cmc_install_packageconfig(${TARGET_NAME} ${TARGET_NAME}Targets ${PROJECT_NAME}
                          FIND_FILE CMakeFinds.cmake.in
                          NAMESPACE ad::)

