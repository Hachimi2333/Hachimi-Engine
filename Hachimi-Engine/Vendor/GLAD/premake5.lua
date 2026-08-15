-- GLAD (OpenGL 4.6 Core) static library project.

project "GLAD"
    kind "StaticLib"
    language "C"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/include"
    }

    files
    {
        "%{prj.location}/src/gl.c"
    }
