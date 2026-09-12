# Tests: headless verification suite built on doctest.
#
# Every suite here runs without a window and without an OpenGL context: package round
# trips, the virtual file system, the project/export pipeline, the scene model and its
# serialization, and the pure engine utilities. Geometry lives in CPU-side MeshData, so
# constructing a Scene no longer needs a GL context and the ECS is covered headlessly.
# The one rule that stays: nothing here may create a window or upload a GPU resource
# (MeshLibrary::GetOrCreate, Texture2D::Create, Shader::Create ...), because that is
# what needs a context.
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
    Renderer/FrameUniformsTests.cpp
    Renderer/MeshFactoryTests.cpp
    Renderer/RenderPipelineTests.cpp
    Scene/RenderViewTests.cpp
    Scene/SceneTests.cpp
    Serialization/ProjectSerializerTests.cpp
    Serialization/SceneSerializerTests.cpp
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
