# Hachimi-Tests: headless verification target.
#
# The engine forbids automated GUI testing, so package round-trip correctness and
# virtual file system behaviour are verified here through a console executable whose
# exit code is the result. It needs the shaders and the UI font next to the executable
# because it packages real engine resources.

add_executable(Hachimi-Tests Source/Main.cpp)

target_include_directories(Hachimi-Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(Hachimi-Tests
    PRIVATE
        Hachimi-Engine
        Ole32
        Shell32
        Uuid
)

set_common_compile_options(Hachimi-Tests)
add_dependencies(Hachimi-Tests copy_runtime_resources)

enable_testing()
add_test(NAME package-roundtrip COMMAND Hachimi-Tests)
