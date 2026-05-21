cmake_minimum_required(VERSION 3.22.1)
project("openglndkdemo")

# ── FreeType via FetchContent ─────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG        VER-2-13-2
    GIT_SHALLOW    TRUE
)
# Disable optional dependencies so FreeType builds with no external libs.
set(FT_DISABLE_ZLIB     TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   TRUE CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freetype)

# ── Main library ──────────────────────────────────────────────────────────────
add_library(openglndkdemo SHARED
    renderer.cpp
    demo_scene.cpp
    ui/font_atlas.cpp
    ui/ui_renderer.cpp
    ui/widget.cpp
    ui/ui_system.cpp
)

target_include_directories(openglndkdemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(log-lib     log)
find_library(android-lib android)

target_link_libraries(
    openglndkdemo
    freetype
    GLESv2
    EGL
    ${android-lib}
    ${log-lib}
)
