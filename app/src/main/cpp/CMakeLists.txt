cmake_minimum_required(VERSION 3.22.1)
project("openglndkdemo")

add_library(openglndkdemo SHARED renderer.cpp)

find_library(log-lib log)

target_link_libraries(
    openglndkdemo
    GLESv2
    EGL
    ${log-lib}
)
