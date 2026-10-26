# CPU-only build tool; it must not depend on copy_runtime_resources (that target consumes its output).
add_executable(BakeEnvironment
    Source/BakeEnvironment.cpp
    ${PROJECT_SOURCE_DIR}/Hachimi-Engine/Source/Core/Log.cpp
    ${PROJECT_SOURCE_DIR}/Hachimi-Engine/Source/Renderer/EnvironmentMapData.cpp
    ${PROJECT_SOURCE_DIR}/Hachimi-Engine/Source/Renderer/EnvironmentMapDataIO.cpp
)
target_include_directories(BakeEnvironment PRIVATE ${PROJECT_SOURCE_DIR}/Hachimi-Engine/Source)
target_compile_definitions(BakeEnvironment PRIVATE SPDLOG_COMPILED_LIB)
target_link_libraries(BakeEnvironment PRIVATE glm::glm spdlog::spdlog)
set_common_compile_options(BakeEnvironment)

set(baked_environment "${CMAKE_BINARY_DIR}/Generated/Environment/Default.henv")
add_custom_command(OUTPUT "${baked_environment}"
    COMMAND $<TARGET_FILE:BakeEnvironment> "${baked_environment}"
    DEPENDS BakeEnvironment
    COMMENT "Baking the default sky environment"
    VERBATIM
)
add_custom_target(bake_environment DEPENDS "${baked_environment}")
add_dependencies(copy_runtime_resources bake_environment)
