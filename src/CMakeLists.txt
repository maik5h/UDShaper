add_library(src STATIC UDShaperElements.cpp gui_w32.cpp assets.cpp UDShaper.cpp GUILayout.cpp logging.cpp contextMenus.cpp)
target_link_libraries(src PRIVATE clap)

target_include_directories(src PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")