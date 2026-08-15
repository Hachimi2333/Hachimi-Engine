-- ImGuizmo static library project.

project "ImGuizmo"
    kind "StaticLib"
    language "C++"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/src",
        "%{wks.location}/Hachimi-Engine/Vendor/imgui/src"
    }

    files
    {
        "%{prj.location}/src/ImGuizmo.cpp"
    }

    links { "ImGui" }
