set(TARGET_NAME ch10_area_lights)

set(${TARGET_NAME}_HEADERS
    CameraSystem.h
    Engine.h
    Scene.h
    Ui.h
)

set(${TARGET_NAME}_SOURCES
    CameraSystem.cpp
    Scene.cpp
    main.cpp
)

find_package(graphics REQUIRED COMPONENTS ad::graphics ad::renderer)
find_package(math REQUIRED COMPONENTS ad::math)
find_package(Rtr_prototypes REQUIRED COMPONENTS scenic)
#find_package(glfw3 3.3 REQUIRED)
#find_package(glad REQUIRED)

add_executable(${TARGET_NAME}
    ${${TARGET_NAME}_SOURCES}
    ${${TARGET_NAME}_HEADERS}
)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ad::engine
        ad::reflect
        ad::ui # TODO: replace with the graphics/imguiui when moved back

        ad::graphics
        ad::math
        ad::renderer
        ad::scenic
)


# Generate assets.json file next to the executable
get_target_property(RENDERER_SOURCE_DIR ad::engine SOURCE_DIR)
#set(REPO_FOLDER ${PROJECT_SOURCE_DIR})
file(GENERATE
     OUTPUT $<TARGET_FILE_DIR:${TARGET_NAME}>/assets.json
     # TODO: should we remove the snacman local V2_resources folder?
     #       for the moment we cannot, we have dedicated custom shaders for snacman (not the base renderer versions)
     CONTENT "{\"prefixes\": [\"${RENDERER_SOURCE_DIR}/resources/\"
                             ]}")
     #                        \"${REPO_FOLDER}/../assets/\"]}")


