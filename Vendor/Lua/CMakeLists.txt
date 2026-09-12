# Lua 5.4 static library.
#
# Upstream Lua ships only a Makefile, so this file is owned by Hachimi-Engine, not by
# Lua, and no Lua source file is modified.
#
# The whole runtime is built from onelua.c, the official single-file build of the
# standard interpreter library. MAKE_LIB tells onelua.c to expose the library interface
# instead of the standalone `lua` interpreter's main().
#
# The Lua distribution is flat: its sources sit next to this file.

add_library(lua STATIC onelua.c)

# The engine and sol2 both include the Lua headers directly, so the include directory is
# part of the public interface. Lua's own headers are found relative to it.
target_include_directories(lua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(lua PUBLIC MAKE_LIB)

# Lua is C89-era code and does not survive /W4.
target_compile_options(lua PRIVATE $<$<C_COMPILER_ID:MSVC>:/W0>)

# Export the locations sol2's find_package(Lua) path looks for, so adding this target is
# enough for sol2 to resolve its Lua dependency without downloading its own copy.
set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Lua include directory" FORCE)
set(LUA_LIBRARIES lua CACHE STRING "Lua libraries" FORCE)
set(LUA_VERSION_STRING "5.4.8" CACHE STRING "Lua version" FORCE)
set(LUA_FOUND TRUE CACHE BOOL "Lua found" FORCE)

add_library(lua::lua ALIAS lua)
