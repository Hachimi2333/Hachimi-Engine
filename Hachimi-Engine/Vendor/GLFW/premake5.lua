-- GLFW static library project.
-- Only the Windows/Win32 backend is compiled.

project "GLFW"
    kind "StaticLib"
    language "C"
    warnings "Off"

    includedirs
    {
        "%{prj.location}/include"
    }

    files
    {
        "%{prj.location}/src/context.c",
        "%{prj.location}/src/egl_context.c",
        "%{prj.location}/src/init.c",
        "%{prj.location}/src/input.c",
        "%{prj.location}/src/monitor.c",
        "%{prj.location}/src/null_init.c",
        "%{prj.location}/src/null_joystick.c",
        "%{prj.location}/src/null_monitor.c",
        "%{prj.location}/src/null_window.c",
        "%{prj.location}/src/osmesa_context.c",
        "%{prj.location}/src/platform.c",
        "%{prj.location}/src/vulkan.c",
        "%{prj.location}/src/wgl_context.c",
        "%{prj.location}/src/win32_init.c",
        "%{prj.location}/src/win32_joystick.c",
        "%{prj.location}/src/win32_module.c",
        "%{prj.location}/src/win32_monitor.c",
        "%{prj.location}/src/win32_thread.c",
        "%{prj.location}/src/win32_time.c",
        "%{prj.location}/src/win32_window.c",
        "%{prj.location}/src/window.c"
    }

    defines { "_GLFW_WIN32", "UNICODE", "_UNICODE" }

    links { "gdi32" }
