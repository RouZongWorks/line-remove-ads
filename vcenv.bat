@echo off
rem Locates Visual Studio (or the Build Tools) and initialises the x64 compiler environment.
set "VSROOT="
set "PF=%ProgramFiles%"
set "PF86=%ProgramFiles(x86)%"

for %%v in (18 17 2022 2019) do call :probe %%v
if defined VSROOT goto :found
echo Could not find Visual Studio with the C++ x64 tools installed.
exit /b 1

:probe
for %%e in (Community Professional Enterprise BuildTools) do call :try %1 %%e
exit /b 0

:try
if defined VSROOT exit /b 0
if exist "%PF%\Microsoft Visual Studio\%1\%2\VC\Auxiliary\Build\vcvars64.bat" set "VSROOT=%PF%\Microsoft Visual Studio\%1\%2"
if defined VSROOT exit /b 0
if exist "%PF86%\Microsoft Visual Studio\%1\%2\VC\Auxiliary\Build\vcvars64.bat" set "VSROOT=%PF86%\Microsoft Visual Studio\%1\%2"
exit /b 0

:found
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
exit /b %errorlevel%
