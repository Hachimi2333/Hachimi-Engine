# Tests: headless verification suite built on doctest.
#
# Every suite here runs without a window and without an OpenGL context: package round
# trips, the virtual file system, the project/export pipeline and the pure engine
# utilities. Anything that constructs a Scene needs a GL context (Scene's constructor
# builds a mesh through VertexArray::Create), so it stays out of this target and is left
# to interactive verification.
#
# Shared fixtures live in Support/. Run the result with Build/<preset>/bin/Tests.exe,
# optionally filtered with -ts=<suite> or -tc=<pattern>.

set(TEST_SOURCES
    Main.cpp
    Support/TestWorkspace.h
    Support/TestWorkspace.cpp
    Core/JobSystemTests.cpp
    Core/UUIDTests.cpp
    Math/MathTests.cpp
    Packaging/BuildSettingsTests.cpp
    Packaging/PackageCorruptionTests.cpp
    Packaging/PackageFormatTests.cpp
    Packaging/PackageReaderTests.cpp
    Packaging/PackageWriterTests.cpp
    Packaging/ProjectPackagerTests.cpp
    Serialization/ProjectSerializerTests.cpp
    Utils/AsyncFileReadTests.cpp
    Utils/FileSystemTests.cpp
    Utils/VirtualFileSystemTests.cpp
    Utils/XXHashTests.cpp
)

add_executable(Tests ${TEST_SOURCES})

target_include_directories(Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Tests
    PRIVATE
        Hachimi-Engine
        doctest::doctest
        Ole32
        Shell32
        Uuid
)

set_common_compile_options(Tests)

# The export suite packages the engine shaders and fonts sitting next to the executable,
# which is what copy_runtime_resources puts there.
add_dependencies(Tests copy_runtime_resources)

add_test(NAME tests COMMAND Tests)
