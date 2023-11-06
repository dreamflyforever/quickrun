cmake_minimum_required(VERSION 3.4.1)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
# project name
project(quickrun)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install)

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# rknn api
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/3rdparty/runtime)
set(RKNN_RT_LIB ${RKNN_API_PATH}/lib/librknnrt.so)

include_directories(${RKNN_API_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/3rdparty)

## opencv
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/opencv/share/OpenCV)
find_package(OpenCV REQUIRED)

#rga
set(RGA_PATH ${CMAKE_SOURCE_DIR}/3rdparty/rga)
set(RGA_LIB ${RGA_PATH}/lib/librga.so)
include_directories( ${RGA_PATH}/include)

set(CMAKE_INSTALL_RPATH "lib")

# quickrun
include_directories( ${CMAKE_SOURCE_DIR}/kernel/include)

#v4l2
include_directories( ${CMAKE_SOURCE_DIR}/libv4l2)

#getusbdev
include_directories( ${CMAKE_SOURCE_DIR}/getusbdevice)

#static queue
include_directories( ${CMAKE_SOURCE_DIR}/queue)

#mpp
include_directories("/usr/include")
include_directories("/usr/include/libdrm")


add_executable(quickrun
        kernel/src/core.cpp
        kernel/src/postprocess.cpp
        libv4l2/v4l2_wrap.cpp
        libv4l2/capture.cpp
	getusbdevice/find_usbdevice.cpp
	queue/static_q.cpp
	mpp/encode/jpg2yuv.cpp
	mpp/encode/utils.cpp
	main.cc
)

target_link_libraries(quickrun
  ${RKNN_RT_LIB}
  ${RGA_LIB}
  ${OpenCV_LIBS}
  -lrockchip_mpp  pthread drm
)


# install target and libraries
install(TARGETS quickrun DESTINATION ./)

install(PROGRAMS ${RKNN_RT_LIB} DESTINATION lib)
install(PROGRAMS ${RGA_LIB} DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)
