cmake_minimum_required(VERSION 3.14)

project(gstmyf2f VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)

#Purpose: Ensures that all targets (especially libraries) 
#are built as position-independent code (PIC).
#Effect: PIC is necessary for shared libraries 
#on most platforms (like Linux). 
#It allows the code to be loaded at any memory address, 
#which is required for dynamic linking.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-base-1.0)

set(SOURCES gstmyf2f.cpp)
add_library(gstmyf2f SHARED ${SOURCES})

target_include_directories(gstmyf2f PRIVATE ${GST_INCLUDE_DIRS})
target_link_libraries(gstmyf2f ${GST_LIBRARIES})

# GStreamer expects libgst<name>.so
set_target_properties(gstmyf2f PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

# Install to standard plugin dir for GStreamer
install(TARGETS gstmyf2f LIBRARY DESTINATION lib/gstreamer-1.0)