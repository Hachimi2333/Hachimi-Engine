# EmitPackage: writes a synthetic game package for the Player's headless modes.
#
# The Player's --verify/--list/--stats paths can be checked against a real artifact without
# launching the editor. The tool packages the shaders and fonts sitting next to it, exactly
# like an export does, which is why it depends on copy_runtime_resources.

add_executable(EmitPackage Source/EmitPackage.cpp)

target_include_directories(EmitPackage PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(EmitPackage
    PRIVATE
        Hachimi-Engine
        Ole32
        Shell32
        Uuid
)

set_common_compile_options(EmitPackage)
add_dependencies(EmitPackage copy_runtime_resources)
