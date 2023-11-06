cmake_minimum_required(VERSION 2.8)

project(mpp_linux_demo)

#include_directories("/usr/local/include/rockchip")
include_directories("/usr/include")
include_directories("/usr/include/libdrm")

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES 
		jpg2yuv.cpp
		utils.cpp
		)


add_executable(mpp_linux_demo ${SOURCE_FILES})
target_link_libraries(mpp_linux_demo  -lrockchip_mpp  pthread drm )
