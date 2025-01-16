cmake_minimum_required(VERSION 3.10)

# Set the project name
project(gstanalyticsaggregator VERSION 1.0)

# Find GStreamer package
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

# Add DeepStream SDK include directory
include_directories(/opt/nvidia/deepstream/deepstream/sources/includes)

# Add the source files
set(SOURCES
    gstanalyticsaggregator.cpp
    gstanalyticsaggregator.h
)

# Add the executable
add_library(gstanalyticsaggregator SHARED ${SOURCES})

# Include directories
target_include_directories(gstanalyticsaggregator PRIVATE ${GST_INCLUDE_DIRS})

# Link libraries
target_link_libraries(gstanalyticsaggregator ${GST_LIBRARIES})

# Install the library
install(TARGETS gstanalyticsaggregator
LIBRARY DESTINATION lib
ARCHIVE DESTINATION lib)