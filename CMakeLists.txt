cmake_minimum_required(VERSION 3.10)
project(UDShaper)

#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
add_library(UDShaper MODULE main.cpp)

target_link_libraries(UDShaper PUBLIC src)

target_include_directories(UDShaper PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/GUI_utils")

set_target_properties(UDShaper PROPERTIES PREFIX "" SUFFIX ".clap")