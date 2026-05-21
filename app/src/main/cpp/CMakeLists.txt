cmake_minimum_required(VERSION 3.22.1)
project("openglndkdemo")

add_library(openglndkdemo SHARED
    renderer.cpp
    demo_scene.cpp
    ui/ui_renderer.cpp
    ui/widget.cpp
    ui/ui_system.cpp
)

target_include_directories(openglndkdemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib log)

target_link_libraries(
    openglndkdemo
    GLESv2
    EGL
    ${log-lib}
)
