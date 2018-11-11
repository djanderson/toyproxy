project(webproxy C)

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_C_STANDARD 11)

###############################################################################
# Set build features
set(CMAKE_BUILD_TYPE Debug)

include(CTest)

add_subdirectory(src)
add_subdirectory(vendor)
add_subdirectory(tests)
