@echo off
rem Regenerates assets\icon.ico and assets\icon.png. Requires the MSVC toolchain.
rem Run it only when the icon artwork changes; the generated files are committed.
setlocal
call "%~dp0..\..\vcenv.bat" || exit /b 1
cd /d "%~dp0"
if not exist ..\..\build mkdir ..\..\build
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 make_icon.cpp /Fo:..\..\build\ /Fe:..\..\build\make_icon.exe /link /SUBSYSTEM:CONSOLE || exit /b 1
..\..\build\make_icon.exe "%~dp0..\..\assets" || exit /b 1
endlocal
