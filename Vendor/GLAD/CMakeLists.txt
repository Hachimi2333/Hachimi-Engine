# GLAD static library.
#
# Upstream GLAD is a generator: the files checked in here under include/ and src/ are
# its generated output (OpenGL 4.6 Core, gl.h loader), and GLAD ships no CMakeLists.txt
# of its own. This file is therefore owned by Hachimi-Engine, not by GLAD, and no GLAD
# source file is modified.

add_library(glad STATIC src/gl.c)

target_include_directories(glad PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Generated loader code is not written to survive /W4.
target_compile_options(glad PRIVATE $<$<C_COMPILER_ID:MSVC>:/W0>)

add_library(glad::glad ALIAS glad)
