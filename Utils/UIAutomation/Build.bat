@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl.exe /nologo /EHsc /std:c++20 /utf-8 UIAutomation.cpp user32.lib /Fe:UIAutomation.exe
