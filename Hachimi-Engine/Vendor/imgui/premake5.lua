-- Dear ImGui (docking branch) static library project.
-- Only the GLFW + OpenGL3 backends needed by the engine are compiled.

project "ImGui"
    kind "StaticLib"
    language "C++"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/src",
        "%{prj.location}/src/backends",
        "%{wks.location}/Hachimi-Engine/Vendor/GLFW/include",
        "%{wks.location}/Hachimi-Engine/Vendor/GLAD/include"
    }

    files
    {
        "%{prj.location}/src/imgui.cpp",
        "%{prj.location}/src/imgui_demo.cpp",
        "%{prj.location}/src/imgui_draw.cpp",
        "%{prj.location}/src/imgui_tables.cpp",
        "%{prj.location}/src/imgui_widgets.cpp",
        "%{prj.location}/src/backends/imgui_impl_glfw.cpp",
        "%{prj.location}/src/backends/imgui_impl_opengl3.cpp"
    }

    defines { "IMGUI_IMPL_OPENGL_LOADER_GLAD" }

    links { "GLAD" }
