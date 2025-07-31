set(TARGET_NAME sandbox)

set(${TARGET_NAME}_HEADERS
    Rgba8ui.h
)

set(${TARGET_NAME}_SOURCES
    main.cpp
)

file(GLOB_RECURSE ${TARGET_NAME}_RESOURCES "resources/*")

source_group(TREE ${CMAKE_CURRENT_LIST_DIR}
             FILES
                ${${TARGET_NAME}_HEADERS}
                ${${TARGET_NAME}_SOURCES}
                ${${TARGET_NAME}_RESOURCES}
)


add_executable(${TARGET_NAME}
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
	${${TARGET_NAME}_RESOURCES}
)


find_package(graphics REQUIRED COMPONENTS ad::graphics ad::renderer)
find_package(math REQUIRED COMPONENTS ad::math)
find_package(Rtr_prototypes REQUIRED COMPONENTS scenic)

find_package(spdlog REQUIRED)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ad::engine
        ad::reflect
        ad::ui # TODO: replace with the graphics/imguiui when moved back

        ad::graphics
        ad::math
        ad::renderer
        ad::scenic

        spdlog::spdlog
)


# Generate assets.json file next to the executable
get_target_property(RENDERER_SOURCE_DIR ad::engine SOURCE_DIR)
#set(REPO_FOLDER ${PROJECT_SOURCE_DIR})
file(GENERATE
     OUTPUT $<TARGET_FILE_DIR:${TARGET_NAME}>/assets.json
     # TODO: should we remove the snacman local V2_resources folder?
     #       for the moment we cannot, we have dedicated custom shaders for snacman (not the base renderer versions)
     CONTENT "{\"prefixes\": [\"${CMAKE_CURRENT_SOURCE_DIR}/resources\",
			      \"${RENDERER_SOURCE_DIR}/resources/\"
                             ]}")

