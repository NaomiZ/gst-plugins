cmake_minimum_required(VERSION 3.10)

project(gstvideostabilizer)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-base-1.0)
find_package(OpenCV REQUIRED)

include_directories(
    ${GST_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

link_directories(
    ${GST_LIBRARY_DIRS}
)

add_library(gstvideostabilizer SHARED
    gstvideostabilizer.cpp
    gstvideostabilizer.h
)

target_link_libraries(gstvideostabilizer
    ${GST_LIBRARIES}
    ${OpenCV_LIBS}
)

target_compile_definitions(gstvideostabilizer PRIVATE PACKAGE=\"videostabilizer\")

install(TARGETS gstvideostabilizer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0
)
