-- Lua 5.4 static library project.
-- Compiles the official onelua.c amalgamation as a library (no interpreter main).

project "Lua"
    kind "StaticLib"
    language "C"
    cdialect "C17"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/include"
    }

    files
    {
        "%{prj.location}/include/**.h",
        "%{prj.location}/src/*.h",
        "%{prj.location}/src/onelua.c"
    }

    defines { "MAKE_LIB" }
