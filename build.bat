@echo off
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

where cmake >nul 2>nul
if errorlevel 1 goto no_cmake

if not exist "%VSWHERE%" goto no_vs
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath >nul 2>nul
if errorlevel 1 goto no_vs

echo configuring
cmake -S . -B build -A x64
if errorlevel 1 goto failed

echo building
cmake --build build --config Release
if errorlevel 1 goto failed

echo testing
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 echo WARNING tests failed, the binaries were still produced

echo.
echo built:
echo   %cd%\build\Release\rr_probe.exe
echo   %cd%\build\Release\rr_overlay.exe
echo.
echo rerun this script after every edit to src\offsets.hpp
endlocal
exit /b 0

:no_cmake
echo cmake is not in PATH.
echo install it with:  winget install Kitware.CMake
exit /b 1

:no_vs
echo the MSVC x64 toolset was not found.
echo install it with:  winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
exit /b 1

:failed
echo build failed, see the output above
exit /b 1
