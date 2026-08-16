-- Hachimi-Engine workspace script.
-- Generates a Visual Studio 2026 solution containing the engine core and the editor.

local outputRoot = "%{wks.location}/Bin"

-- Include a project script only when it exists so the workspace can grow incrementally.
local function includeIfPresent(path)
    if os.isfile(path) then
        include(path)
    end
end

workspace "Hachimi-Engine"
    configurations { "Debug", "Release" }
    architecture "x86_64"
    startproject "Hachimi-Editor"

    filter "system:windows"
        system "windows"
        characterset "Unicode"
        buildoptions { "/utf-8" }
        defines { "HE_PLATFORM_WINDOWS" }

    filter {}
        cppdialect "C++20"
        staticruntime "On"
        multiprocessorcompile "On"
        targetdir(outputRoot .. "/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}")
        objdir(outputRoot .. "/Obj/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}")

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
        runtime "Debug"

    filter "configurations:Release"
        symbols "On"
        optimize "Speed"
        runtime "Release"

    filter {}

includeIfPresent("Hachimi-Engine/Vendor/Box3D/premake5.lua")
includeIfPresent("Hachimi-Engine/Vendor/GLFW/premake5.lua")
includeIfPresent("Hachimi-Engine/Vendor/GLAD/premake5.lua")
includeIfPresent("Hachimi-Engine/Vendor/imgui/premake5.lua")
includeIfPresent("Hachimi-Engine/Vendor/spdlog/premake5.lua")
includeIfPresent("Hachimi-Engine/Vendor/yaml-cpp/premake5.lua")
includeIfPresent("Hachimi-Editor/Vendor/ImGuiFileDialog/premake5.lua")
includeIfPresent("Hachimi-Editor/Vendor/ImGuizmo/premake5.lua")

includeIfPresent("Hachimi-Engine/premake5.lua")
includeIfPresent("Hachimi-Editor/premake5.lua")
