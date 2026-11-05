@echo off
rem Builds "bin\Line Remove Ads.exe" (x64, static CRT, no console window).
rem Requires Visual Studio or the Build Tools with the "Desktop development with C++" workload.
setlocal
call "%~dp0vcenv.bat" || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
if not exist bin mkdir bin

rc /nologo /fo build\app.res src\app.rc || exit /b 1
cl /nologo /std:c++17 /O2 /GL /MT /EHsc /utf-8 /DUNICODE /D_UNICODE /W3 ^
   src\main.cpp build\app.res /Fo:build\ /Fe:"bin\Line Remove Ads.exe" ^
   /link /SUBSYSTEM:WINDOWS /DEBUG:NONE /LTCG /OPT:REF /OPT:ICF ^
   /MANIFEST:EMBED /MANIFESTINPUT:src\app.manifest || exit /b 1

echo.
echo Built: bin\Line Remove Ads.exe
endlocal
