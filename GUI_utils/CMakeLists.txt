add_library(GUI_utils STATIC shapeEditor.cpp assets.cpp)

target_include_directories(GUI_utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")