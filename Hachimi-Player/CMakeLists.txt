# Hachimi-Player standalone game runtime, launched by exported builds.

set(PLAYER_SOURCES
    Source/PlayerApplication.cpp
    Source/PlayerApplication.h
    Source/PlayerLayer.cpp
    Source/PlayerLayer.h
)

add_executable(Hachimi-Player ${PLAYER_SOURCES})

target_include_directories(Hachimi-Player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(Hachimi-Player
    PRIVATE
        Hachimi-Engine
        Ole32
        Shell32
        Uuid
)

set_common_compile_options(Hachimi-Player)
add_dependencies(Hachimi-Player copy_runtime_resources)
