@echo off
rem Regenerate from this game's ROM/config through the configured shared compiler.
setlocal
set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "BUILD_DIR=%~dp0build"
if not "%~1"=="" set "BUILD_DIR=%~f1"
if not exist "%BUILD_DIR%\CMakeCache.txt" (
  echo ERROR: configure this game first, with your ROM at game\sonic2.bin.
  echo   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  echo Optional: regen.bat path-to-another-configured-build
  exit /b 1
)
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target genesisrecomp_generate_sonic2
exit /b %ERRORLEVEL%
