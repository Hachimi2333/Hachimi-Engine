cmake_minimum_required(VERSION 3.16)
project(ImGuizmo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(IMGUIZMO_STANDALONE TRUE)
else()
    set(IMGUIZMO_STANDALONE FALSE)
endif()

option(IMGUIZMO_BUILD_EXAMPLE "Build the example application" ${IMGUIZMO_STANDALONE})

add_library(imguizmo STATIC
    src/ImGuizmo.cpp
    src/GraphEditor.cpp
    src/ImCurveEdit.cpp
    src/ImGradient.cpp
    src/ImSequencer.cpp
    src/ImVectorEditor.cpp
)

add_library(imguizmo::imguizmo ALIAS imguizmo)

target_include_directories(imguizmo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

if(IMGUIZMO_BUILD_EXAMPLE)
    include(FetchContent)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
    target_link_libraries(imguizmo PUBLIC imgui)
else()
    # --- Hachimi-Engine addition (the only change to this file) ------------------
    # Upstream only links ImGuizmo against ImGui inside the example branch above, so a
    # consumer that supplies its own imgui target would otherwise build ImGuizmo without
    # any imgui include directory and fail on #include "imgui.h". Link against the
    # consuming project's imgui target instead of fetching a second copy from GitHub.
    if(TARGET imgui)
        target_link_libraries(imguizmo PUBLIC imgui)
    endif()
    # ----------------------------------------------------------------------------
endif()

if(IMGUIZMO_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
