-- yaml-cpp static library project.

project "yaml-cpp"
    kind "StaticLib"
    language "C++"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/include"
    }

    files
    {
        "%{prj.location}/src/**.cpp"
    }

    defines { "YAML_CPP_STATIC_DEFINE" }
