@echo off
rem Builds the executable and creates dist\LineRemoveAds-v<version>-win-x64.zip.
setlocal
set "VERSION=1.0.0"
cd /d "%~dp0"
call "%~dp0build.bat" || exit /b 1

set "STAGE=dist\stage\Line Remove Ads"
if exist dist rmdir /s /q dist
mkdir "%STAGE%"
copy /y "bin\Line Remove Ads.exe" "%STAGE%\" >nul
copy /y README.md "%STAGE%\" >nul
copy /y LICENSE "%STAGE%\" >nul
powershell -NoProfile -Command "Compress-Archive -Path 'dist\stage\Line Remove Ads' -DestinationPath 'dist\LineRemoveAds-v%VERSION%-win-x64.zip' -Force" || exit /b 1
rmdir /s /q dist\stage
echo.
echo Created: dist\LineRemoveAds-v%VERSION%-win-x64.zip
endlocal
