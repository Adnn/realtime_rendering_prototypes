include(cmc-cpp)

add_subdirectory(libs/scenic/scenic)

add_subdirectory(apps/ch10_area_lights/ch10_area_lights)

if(BUILD_TESTING)
    #add_subdirectory()
endif()
