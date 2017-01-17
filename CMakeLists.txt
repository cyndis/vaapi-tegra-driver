cmake_minimum_required(VERSION 2.6)
project(vicva)

find_package(PkgConfig)
pkg_search_module(DRM REQUIRED libdrm)

add_library(tegra_drv_video MODULE main.cpp objects.cpp gem.cpp vic.cpp)
set_target_properties(tegra_drv_video PROPERTIES PREFIX "")
set_target_properties(tegra_drv_video PROPERTIES CXX_STANDARD 14)
set_target_properties(tegra_drv_video PROPERTIES CXX_STANDARD_REQUIRED ON)

target_link_libraries(tegra_drv_video ${DRM_LIBRARIES})
target_include_directories(tegra_drv_video PUBLIC ${DRM_INCLUDE_DIRS})

install(TARGETS tegra_drv_video LIBRARY DESTINATION lib)
