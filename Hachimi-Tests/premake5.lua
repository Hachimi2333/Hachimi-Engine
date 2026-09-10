-- Hachimi-Tests: headless verification target.
-- The engine forbids automated GUI testing, so package round-trip correctness and
-- virtual file system behaviour are verified here through a console executable
-- whose exit code is the result.
project "Hachimi-Tests"
    kind "ConsoleApp"
    language "C++"

    includedirs
    {
        "%{prj.location}/Source",
        "%{wks.location}/Hachimi-Engine/Source",
        "%{wks.location}/Hachimi-Engine/Vendor/EnTT/src",
        "%{wks.location}/Hachimi-Engine/Vendor/EnTT/single_include",
        "%{wks.location}/Hachimi-Engine/Vendor/glm",
        "%{wks.location}/Hachimi-Engine/Vendor/spdlog/include",
        "%{wks.location}/Hachimi-Engine/Vendor/yaml-cpp/include",
        "%{wks.location}/Hachimi-Engine/Vendor/zstd/lib"
    }

    files
    {
        "%{prj.location}/Source/**.h",
        "%{prj.location}/Source/**.cpp"
    }

    defines
    {
        "SPDLOG_COMPILED_LIB",
        "YAML_CPP_STATIC_DEFINE",
        "GLM_ENABLE_EXPERIMENTAL"
    }

    links
    {
        "Hachimi-Engine",
        "zstd",
        "spdlog",
        "yaml-cpp",
        "Box3D",
        "Lua",
        "GLFW",
        "GLAD",
        "opengl32",
        "Ole32",
        "Shell32",
        "Uuid"
    }
