set(TARGET_NAME ch11_voxel)

set(${TARGET_NAME}_HEADERS
    DebugDrawing.h
    Engine.h
    FrameGraph.h
    Scene.h
    SetupDrawing.h
    ShaderConstants.h
    Ui.h
    UniformSetterWitness.h
    Voxelization.h

    debug/DebugDrawer.h
    debug/DebugRenderer.h

    log/Logging.h
    log/Logging-channels.h
    log/Logging-init.h
)

set(${TARGET_NAME}_SOURCES
    FrameGraph.cpp
    main.cpp
    Scene.cpp
    SetupDrawing.cpp
    ShaderConstants.cpp
    Voxelization.cpp

    debug/DebugDrawer.cpp
    debug/DebugRenderer.cpp

    log/Logging-init.cpp
)

set(${TARGET_NAME}_INFRA
    README.md
)

file(GLOB_RECURSE ${TARGET_NAME}_RESOURCES "resources/*")

source_group(/ FILES ${${TARGET_NAME}_INFRA})

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
	${${TARGET_NAME}_INFRA}
)


find_package(graphics REQUIRED COMPONENTS ad::graphics ad::renderer)
find_package(handy REQUIRED COMPONENTS ad::handy)
find_package(math REQUIRED COMPONENTS ad::math)
find_package(Rtr_prototypes REQUIRED COMPONENTS scenic)

find_package(spdlog REQUIRED)

target_link_libraries(${TARGET_NAME}
    PRIVATE
        ad::engine
        ad::handy
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
set(REPO_FOLDER ${PROJECT_SOURCE_DIR})
file(GENERATE
     OUTPUT $<TARGET_FILE_DIR:${TARGET_NAME}>/assets.json
     # Note: we try to locate the assets next to the repository
     CONTENT "{\"prefixes\": [
                    \"${CMAKE_CURRENT_SOURCE_DIR}/resources\",
                    \"${RENDERER_SOURCE_DIR}/resources/\",
                    \"${REPO_FOLDER}/../assets/\"
            ]}")


