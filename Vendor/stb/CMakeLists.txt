# stb header-only libraries.
#
# stb ships headers only and provides no CMakeLists.txt, so this file is owned by
# Hachimi-Engine, not by stb, and no stb source file is modified.
#
# This is an INTERFACE target: it only contributes include directories. The
# STB_IMAGE_IMPLEMENTATION translation unit stays in the engine
# (Hachimi-Engine/Source/Renderer/stb_image.cpp), which is where the implementation
# macro belongs for a header-only library.

add_library(stb INTERFACE)

target_include_directories(stb INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_library(stb::stb ALIAS stb)
