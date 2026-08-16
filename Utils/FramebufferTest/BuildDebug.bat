@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl.exe /nologo /EHsc /std:c++20 /utf-8 /I"D:\Workspace\Cpp\Hachimi-Engine\Hachimi-Engine\Vendor\GLAD\include" /I"D:\Workspace\Cpp\Hachimi-Engine\Hachimi-Engine\Vendor\GLFW\include" FramebufferTestDebug.cpp "D:\Workspace\Cpp\Hachimi-Engine\Bin\Debug-windows-x86_64\GLFW.lib" "D:\Workspace\Cpp\Hachimi-Engine\Bin\Debug-windows-x86_64\GLAD.lib" opengl32.lib user32.lib gdi32.lib shell32.lib /Fe:FramebufferTestDebug.exe
