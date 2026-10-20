@echo off
setlocal
echo ================================================
echo  Subside - Sidechain Ducker
echo  Build: VST3 + CLAP (Windows x64, Release)
echo ================================================
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake not found in PATH.
    exit /b 1
)

if not exist "JUCE\CMakeLists.txt" (
    echo ERROR: vendored JUCE is missing.
    exit /b 1
)
if not exist "clap-juce-extensions\CMakeLists.txt" (
    echo ERROR: vendored clap-juce-extensions is missing.
    exit /b 1
)

if not exist build mkdir build
cd build

echo [1/3] Configuring with CMake ^(Visual Studio 2022, x64^)...
cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

echo.
echo [2/3] Building Release...
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

echo.
echo [3/3] Staging artifacts...
if exist "dist\Subside.clap" echo   CLAP : %CD%\dist\Subside.clap
if exist "dist\Subside.vst3" echo   VST3 : %CD%\dist\Subside.vst3

echo.
echo Build complete.
echo To create the installer, run: installer\build_installer.bat
cd ..
endlocal
