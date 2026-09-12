# Hachimi-Engine build entry point.
#
# Requirements: CMake 3.28 or newer, the Visual Studio 2026 x64 toolset, Windows x86_64.
# Configure and build from a VS 2026 Developer Command Prompt:
#
#   cmake --preset x64-debug
#   cmake --build --preset x64-debug
#
# See README.md for the full list of presets.

cmake_minimum_required(VERSION 3.28)

# Keep debug information editable while debugging and in a plain PDB for release builds.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# This is the ONLY project() call in the whole build tree, and it must stay that way.
#
# Vendor/sol2/CMakeLists.txt sets CMAKE_PROJECT_INCLUDE to a file reached through
# ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Includes/Project.cmake. That variable propagates by
# directory scope, and CMake includes it as the last step of *every* project() call --
# not only the top-level one. A project() call living in a different directory would
# resolve the path against its own PROJECT_SOURCE_DIR, point at a file that does not
# exist, and abort the configure. Keeping project() here in the root directory avoids
# that entirely.
project(Hachimi-Engine
    VERSION 0.1.0
    DESCRIPTION "A C++20 3D game engine and editor built on OpenGL 4.6 Core"
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "Hachimi-Engine currently supports Windows only.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Hachimi-Engine targets x86_64 only; the selected toolchain is 32-bit.")
endif()

# --------------------------------------------------------------------------
# Toolchain
# --------------------------------------------------------------------------

if(MSVC)
    # cl.exe prints the list of headers it read whenever /showIncludes is passed, and ninja is
    # supposed to delete those lines again by matching a prefix that CMake stores for it in
    # CMakeFiles/rules.ninja. That prefix is captured in whatever code page the build tree was
    # configured under, while the compiler writes the localized text in the code page it runs
    # under; the two regularly disagree and then every header list ends up in the log. So the
    # build passes the flag nowhere: it is stripped from the depfile flags below, and the C++20
    # module scan, the only other step that adds it, is switched off right after.
    #
    # What it costs: ninja then learns nothing about headers, so an edited header does not
    # recompile the sources that include it. Use Rebuild rather than Build after touching one.
    string(REPLACE "/showIncludes" "" CMAKE_DEPFILE_FLAGS_C "${CMAKE_DEPFILE_FLAGS_C}")
    string(REPLACE "/showIncludes" "" CMAKE_DEPFILE_FLAGS_CXX "${CMAKE_DEPFILE_FLAGS_CXX}")

    # No source in this project imports a module, so the scan has nothing to find, and skipping
    # it removes the last /showIncludes invocation from the build. Enable it again only for a
    # toolchain that reports in English, where the prefix is plain ASCII and code pages stop
    # mattering.
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

    # The sources and format strings are UTF-8 no matter what the active code page is,
    # the Windows headers are used in their lean form, and min/max stay usable names.
    add_compile_options(/utf-8)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
    add_compile_definitions($<$<CONFIG:Debug>:_CRT_SECURE_NO_WARNINGS>)

    # The whole build uses the dynamic CRT, which is the only runtime Visual Studio
    # redistributes. Vendor/Box3D overrides this for its own target.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# --------------------------------------------------------------------------
# Language and output layout
# --------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Only Debug and Release are supported. The presets choose one; the default keeps a plain
# "cmake -S . -B Build/x64-debug" working as well.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug or Release)" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "Hachimi-Engine builds Debug and Release only, not '${CMAKE_BUILD_TYPE}'.")
endif()

# Executables and the static library land in one directory per build tree; everything
# else stays in CMake's own layout below the binary directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --------------------------------------------------------------------------
# Shared settings for the project's own targets
# --------------------------------------------------------------------------

# Applies the compiler settings every first-party target shares. Third-party targets keep
# their own flags: raising their warning level would only bury real warnings in noise.
function(set_common_compile_options target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE HE_PLATFORM_WINDOWS)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()

# Copies the resources the executables load from disk next to them. Shader and font paths are
# resolved relative to the running executable (see PlatformUtils::GetExecutableDirectory), and
# all executables land in the same output directory, so this runs once for the whole build
# instead of once per target: several copies of the same directory racing each other fail with
# "Permission denied" while they link in parallel.
add_custom_target(copy_runtime_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/Hachimi-Engine/Resources/Shaders"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/Hachimi-Engine/Resources/Fonts"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Assets/Fonts"
    COMMENT "Copying shaders and fonts next to the executables"
    VERBATIM
)

# --------------------------------------------------------------------------
# Third-party library configuration
# --------------------------------------------------------------------------
#
# Library options are set here, in the same directory scope that adds each library,
# because add_subdirectory() gives every library its own scope: a library that calls
# set(CMAKE_CXX_STANDARD ...) or set(CMAKE_MSVC_RUNTIME_LIBRARY ...) for itself cannot
# leak those settings into Hachimi-Engine/ or Hachimi-Editor/.
#
# Each library's own CMakeLists.txt is used as-is. Only the libraries that ship no
# usable one (GLAD, Lua, stb, imgui, zstd) have a thin project-owned CMakeLists.txt
# placed inside their own directory.

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build third-party libraries statically" FORCE)

# GLFW
set(GLFW_BUILD_DOCS     OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS    OFF)
set(GLFW_INSTALL        OFF)

# glm
set(GLM_BUILD_TESTS   OFF)
set(GLM_BUILD_INSTALL OFF)
set(GLM_ENABLE_CXX_20 ON )

# EnTT is a header-only interface target; keep its optional extras disabled.
set(ENTT_INSTALL        OFF)
set(ENTT_BUILD_TESTING  OFF)
set(ENTT_BUILD_TESTBED  OFF)

# Box3D build options (only the library is built; its samples and tests were removed).
set(BOX3D_VALIDATE OFF)

# ImGuizmo
set(IMGUIZMO_BUILD_EXAMPLE OFF)

# nativefiledialog-extended
set(NFD_BUILD_TESTS OFF)
set(NFD_INSTALL     OFF)

# sol2. SOL2_BUILD_LUA must be OFF before the subdirectory is added, otherwise sol2
# runs find_package(LuaBuild REQUIRED) and downloads a second Lua from the network.
# Switching it off makes sol2 discover our own Lua target instead (see Vendor/Lua).
set(SOL2_BUILD_LUA  OFF)
set(SOL2_TESTS      OFF)
set(SOL2_EXAMPLES   OFF)
set(SOL2_ENABLE_INSTALL OFF)

# spdlog
set(SPDLOG_BUILD_ALL        OFF)
set(SPDLOG_BUILD_SHARED     OFF)
set(SPDLOG_BUILD_TESTS      OFF)
set(SPDLOG_BUILD_EXAMPLE    OFF)
set(SPDLOG_BUILD_BENCH      OFF)
set(SPDLOG_INSTALL          OFF)

# yaml-cpp
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_INSTALL     OFF)
set(YAML_CPP_FORMAT_SOURCE OFF)

# zstd
set(ZSTD_BUILD_PROGRAMS    OFF)
set(ZSTD_BUILD_TESTS       OFF)
set(ZSTD_BUILD_CONTRIB     OFF)
set(ZSTD_BUILD_SHARED      OFF)
set(ZSTD_BUILD_STATIC      ON)
set(ZSTD_LEGACY_SUPPORT    OFF)
set(ZSTD_MULTITHREAD_SUPPORT ON)

# Every library is added without EXCLUDE_FROM_ALL. Excluding a subdirectory from the
# default build also makes CMake skip evaluating its targets on Windows, which leaves the
# library targets unavailable to target_link_libraries. Every optional component is
# switched off through each library's own options above instead.
add_subdirectory(Vendor/Box3D)
add_subdirectory(Vendor/EnTT)
add_subdirectory(Vendor/GLAD)
add_subdirectory(Vendor/GLFW)
add_subdirectory(Vendor/Lua)
add_subdirectory(Vendor/NativeFileDialogExtended)
add_subdirectory(Vendor/glm)
add_subdirectory(Vendor/spdlog)
add_subdirectory(Vendor/stb)
add_subdirectory(Vendor/yaml-cpp)
add_subdirectory(Vendor/zstd)

# imgui must precede ImGuizmo: ImGuizmo's CMakeLists.txt links its target against the
# imgui target when one already exists, and simply compiles without it otherwise.
add_subdirectory(Vendor/imgui)
add_subdirectory(Vendor/ImGuizmo)

# sol2 MUST be the last library added.
#
# Its CMakeLists.txt assigns CMAKE_PROJECT_INCLUDE at directory scope, and CMake applies
# that variable to every subsequent project() call in this directory or any directory
# processed after it. Any library added below would therefore try to include sol2's
# project hook against its own PROJECT_SOURCE_DIR and fail. If a new third-party library
# is ever added here, put it above this line.
add_subdirectory(Vendor/sol2)

# --------------------------------------------------------------------------
# First-party targets
# --------------------------------------------------------------------------

add_subdirectory(Hachimi-Engine)
add_subdirectory(Hachimi-Editor)
add_subdirectory(Hachimi-Player)
add_subdirectory(Hachimi-Tests)
