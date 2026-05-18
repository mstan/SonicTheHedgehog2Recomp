@echo off
rem Convenience wrapper — regenerate sonic2_full.c and sonic2_dispatch.c
rem from sonic2.bin using the shared recompiler.
setlocal
set ROOT=%~dp0..\SonicTheHedgehogRecomp\segagenesisrecomp
set GAMEDIR=%ROOT%\sonicthehedgehog2
set RECOMP=%ROOT%\recompiler\build\Release\GenesisRecomp.exe

if not exist "%RECOMP%" (
  echo ERROR: recompiler not built. Build it first:
  echo   cd %ROOT%\recompiler
  echo   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  echo   cmake --build build --config Release
  exit /b 1
)

pushd "%GAMEDIR%"
"%RECOMP%" sonic2.bin --game game.toml --reverse-debug
set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
