@echo off
setlocal
set "ROOT=%~dp0"
set "PREMAKE=%ROOT%Vendor\Premake\Bin\premake5.exe"

if not exist "%PREMAKE%" (
    echo [ERROR] Premake5 executable not found: "%PREMAKE%"
    echo Please place premake5.exe under Vendor\Premake\Bin\.
    exit /b 1
)

cd /d "%ROOT%"
"%PREMAKE%" vs2026 --file=premake5.lua
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to generate the Visual Studio 2026 solution.
    exit /b 1
)

echo.
echo [OK] Hachimi-Engine.sln generated at the repository root.
exit /b 0
