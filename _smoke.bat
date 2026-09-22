@echo off
REM Sonic 2 visual-regression smoke check.
REM Launches the runner with smoke_enter_level_run_right.input + framebuffer
REM hashing every 60 wall frames, compares to the checked-in baseline.
REM
REM Run after `_build_native.bat` whenever you change shared runner code,
REM regen with a recompiler change, or land a fix. A divergence means the
REM visible behaviour changed -- either fix the regression, or refresh
REM the baseline (see commands below).
REM
REM Usage:
REM   _smoke.bat                       # assert vs baseline (exit 1 on diff)
REM   _smoke.bat --write-baseline      # capture current as new baseline
REM   _smoke.bat --keep-log            # save runner stderr to smoke.log
REM
REM Exit codes:
REM   0 -- match
REM   1 -- DIVERGENCE (something visibly changed)
REM   2 -- runner / environment error
REM   3 -- no baseline file present yet (use --write-baseline)
@setlocal
@set "ENGINE_DIR=%~dp0segagenesisrecomp"
@if exist "%~dp0engine-local\tools\zone_smoke.py" set "ENGINE_DIR=%~dp0engine-local"
@if defined GENESIS_RECOMP_ROOT set "ENGINE_DIR=%GENESIS_RECOMP_ROOT%"
@set "ZONE_SMOKE=%ENGINE_DIR%\tools\zone_smoke.py"
@set "INPUT=%~dp0tools\smoke_enter_level_run_right.input"
python "%ZONE_SMOKE%" --game sonic2 --exe "%~dp0build\Release\SonicTheHedgehog2Recomp.exe" --rom "%~dp0game\sonic2.bin" --input "%INPUT%" --hash-frames 60 %*
@endlocal
