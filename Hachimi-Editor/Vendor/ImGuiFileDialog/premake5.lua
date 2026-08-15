-- ImGuiFileDialog static library project.

project "ImGuiFileDialog"
    kind "StaticLib"
    language "C++"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/src",
        "%{prj.location}/src/dirent",
        "%{wks.location}/Hachimi-Engine/Vendor/imgui/src"
    }

    files
    {
        "%{prj.location}/src/ImGuiFileDialog.cpp"
    }

    links { "ImGui" }
