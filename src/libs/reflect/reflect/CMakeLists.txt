set(TARGET_NAME reflect)

set(${TARGET_NAME}_HEADERS
    ReflectHelpers.h
    DearImguiWitness.h
)

set(${TARGET_NAME}_SOURCES
)

set(${TARGET_NAME}_INFRA
    CMakeFinds.cmake.in
)

source_group(/ FILES ${${TARGET_NAME}_INFRA})

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}
             FILES ${${TARGET_NAME}_HEADERS} ${${TARGET_NAME}_SOURCES}
)

add_library(${TARGET_NAME} INTERFACE
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
    ${${TARGET_NAME}_INFRA}
)

add_library(ad::${TARGET_NAME} ALIAS ${TARGET_NAME})


cmc_target_current_include_directory(${TARGET_NAME})

#cmc_cpp_all_warnings_as_errors(${TARGET_NAME} ENABLED ${BUILD_CONF_WarningAsError})

cmc_cpp_sanitizer(${TARGET_NAME} ${BUILD_CONF_Sanitizer})


##
## Dependencies
##

cmc_find_dependencies(FILE CMakeFinds.cmake.in)

target_link_libraries(${TARGET_NAME}
    INTERFACE
        ad::math
        imgui::imgui
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
