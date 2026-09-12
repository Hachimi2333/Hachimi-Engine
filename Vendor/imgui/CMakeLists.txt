# Dear ImGui (docking branch) static library.
#
# Upstream ImGui ships CMakeLists.txt files only for its example applications, not for
# the library itself, so this file is owned by Hachimi-Engine, not by ImGui, and no
# ImGui source file is modified.
#
# Only the two backends this project uses are compiled: GLFW for platform/input and
# OpenGL3 for rendering. Every other backend remains in the tree unmodified.

set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_glfw.cpp
    backends/imgui_impl_opengl3.cpp
    # Not compiled, but listed so it stays visible in IDE project views.
    imgui_demo.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${IMGUI_SOURCES})

# imgui_demo.cpp is deliberately left out of the build because the editor never opens
# the demo window.
set_source_files_properties(imgui_demo.cpp PROPERTIES HEADER_FILE_ONLY TRUE)

target_include_directories(imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/backends>
)

# ImGui's OpenGL3 backend carries its own bundled loader; GLAD is used by the engine's
# renderer instead, so the two never meet in one translation unit.
target_link_libraries(imgui PUBLIC glfw opengl32)

# Third-party UI code is not warning-clean under /W4.
target_compile_options(imgui PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W0>)

add_library(imgui::imgui ALIAS imgui)
