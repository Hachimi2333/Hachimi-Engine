@echo off
setlocal
set "ROOT=%~dp0..\.."
set "BUILD=%ROOT%\Build\x64-release"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl.exe /nologo /EHsc /std:c++20 /utf-8 /MD ^
  /I"%ROOT%\Vendor\GLAD\include" /I"%ROOT%\Vendor\GLFW\include" ^
  FramebufferTest.cpp ^
  "%BUILD%\Vendor\GLFW\src\glfw3.lib" ^
  "%BUILD%\Vendor\GLAD\glad.lib" ^
  opengl32.lib user32.lib gdi32.lib shell32.lib ^
  /Fe:FramebufferTest.exe
exit /b %errorlevel%
