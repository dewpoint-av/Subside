@echo off
setlocal
echo ================================================
echo  Subside - Build installer (Inno Setup)
echo ================================================
echo.

set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo ERROR: Inno Setup 6 not found at:
    echo        %ISCC%
    exit /b 1
)

if not exist "..\build\dist\Subside.clap" (
    echo ERROR: build\dist\Subside.clap not found.
    echo        Run build.bat first.
    exit /b 1
)
if not exist "..\build\dist\Subside.vst3" (
    echo ERROR: build\dist\Subside.vst3 not found.
    echo        Run build.bat first.
    exit /b 1
)

"%ISCC%" "Subside.iss"
if errorlevel 1 (
    echo ERROR: Inno Setup compile failed.
    exit /b 1
)

echo.
echo Installer built. See installer\output\
endlocal
