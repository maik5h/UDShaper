add_library(src STATIC shapeEditor.cpp assets.cpp UDShaper.cpp)

target_include_directories(src PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")