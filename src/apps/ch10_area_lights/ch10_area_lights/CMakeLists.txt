set(TARGET_NAME ch10_area_lights)

set(${TARGET_NAME}_HEADERS
    Scene.h
    shaders.h
)

set(${TARGET_NAME}_SOURCES
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
        ad::graphics
        ad::math
        ad::renderer
        ad::scenic
)
