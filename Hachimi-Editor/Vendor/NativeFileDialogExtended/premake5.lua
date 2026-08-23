-- Native File Dialog Extended static library project.
-- Only the Windows implementation is kept because Hachimi-Engine targets Windows x86_64.

project "NativeFileDialogExtended"
    kind "StaticLib"
    language "C++"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/src/include"
    }

    files
    {
        "%{prj.location}/src/include/nfd.h",
        "%{prj.location}/src/nfd_win.cpp"
    }
