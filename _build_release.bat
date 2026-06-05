@echo off
REM Stripped SHIPPING build: SONIC_REVERSE_DEBUG=OFF (no Tier-1 rdb ring, no
REM reverse-debug instrumentation) and no oracle. Configures + builds the
REM native target into build-release\. package_release.py then zips
REM build-release\Release\SonicTheHedgehog2Recomp.exe.
REM
REM (Day-to-day dev uses _build_native.bat against build\, where the option
REM  defaults ON so the rdb_* debug tools work.)
set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build-release" -G "Visual Studio 17 2022" -A x64 -DSONIC_REVERSE_DEBUG=OFF
"%CMAKE_EXE%" --build "%~dp0build-release" --config Release --target SonicTheHedgehog2Recomp
echo BUILD_EXIT=%ERRORLEVEL%
