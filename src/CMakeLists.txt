include(cmc-cpp)

add_subdirectory(libs/reflect/reflect)
add_subdirectory(libs/ui/ui)
add_subdirectory(libs/engine/engine)
add_subdirectory(libs/scenic/scenic)

add_subdirectory(apps/ch10_area_lights/ch10_area_lights)
add_subdirectory(apps/ch10_ltc/ch10_ltc)
add_subdirectory(apps/ch11_ssao/ch11_ssao)
add_subdirectory(apps/ch11_voxel/ch11_voxel)

if(BUILD_TESTING)
    #add_subdirectory()
endif()
