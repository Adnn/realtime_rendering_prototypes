include(cmc-cpp)

add_subdirectory(libs/reflect/reflect)
add_subdirectory(libs/ui/ui)
add_subdirectory(libs/engine/engine)
add_subdirectory(libs/scenic/scenic)

add_subdirectory(apps/ch10_area_lights/ch10_area_lights)
add_subdirectory(apps/ch10_ltc/ch10_ltc)

if(BUILD_TESTING)
    #add_subdirectory()
endif()
