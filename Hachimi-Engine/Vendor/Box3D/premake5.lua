-- Box3D static library project.
-- Box3D is a C17 library with no dependencies beyond the C runtime.

project "Box3D"
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
        "%{prj.location}/src/**.c",
        "%{prj.location}/src/**.h",
        "%{prj.location}/src/*.inl",
        "%{prj.location}/src/box3d.natvis"
    }
