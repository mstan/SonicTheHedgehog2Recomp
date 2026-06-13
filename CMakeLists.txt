cmake_minimum_required(VERSION 3.16)
project(SonicTheHedgehog2Recomp C)

set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------------------------
# Sonic the Hedgehog 2 native build — sibling to SonicTheHedgehogRecomp/.
#
# All shared engine code (runner) lives in the canonical segagenesisrecomp
# checkout at the workspace root, reached here through a local directory
# junction (./segagenesisrecomp -> ../segagenesisrecomp). Per-game files
# (sonic2_spec.c, sonic2_hybrid_table.c) live alongside the Sonic 2 generated
# TUs in segagenesisrecomp/sonicthehedgehog2/. Sonic 2 does not consume any
# files from Sonic 1 directly.
# ---------------------------------------------------------------------------

# Engine root. Public layout: the committed `segagenesisrecomp` submodule
# (clone --recursive is self-contained). Local-dev override: if a gitignored
# `engine-local` symlink -> ../segagenesisrecomp is present, prefer it so every
# game repo shares ONE engine checkout. Create it with scripts/link-engine.*.
if(EXISTS "${CMAKE_SOURCE_DIR}/engine-local")
    set(RECOMP_ROOT "${CMAKE_SOURCE_DIR}/engine-local")
    message(STATUS "engine: local-dev override (engine-local/ -> shared checkout)")
else()
    set(RECOMP_ROOT "${CMAKE_SOURCE_DIR}/segagenesisrecomp")
    message(STATUS "engine: committed segagenesisrecomp/ submodule")
endif()
set(RUNNER_ROOT "${RECOMP_ROOT}/runner")

# Tier-1 reverse-debugger + oracle Tier-3 instrumentation. ON by default for
# day-to-day Sonic 2 bring-up (the rdb_* TCP commands / WRAM-write ring depend
# on it). Build a stripped shipping binary with -DSONIC_REVERSE_DEBUG=OFF
# (package_release.py does this). The clown68000-interpreter ORACLE target is
# separate from this flag and is never part of a release build either way.
option(SONIC_REVERSE_DEBUG
    "Enable Tier-1 reverse-debugger hooks (WRAM-write ring, rdb_* TCP cmds) and \
the oracle's Tier-3 per-instruction capture. OFF = stripped native build."
    ON)
option(GEN_DEV_TRACE
    "Compile the dev chip/snd trace rings + dumps (NEVER in release builds)" OFF)

# clownmdemu-core is the mstan fork; our changes are committed in it directly
# (no build-time patch step — see THIRD-PARTY-LICENSES.md).
add_subdirectory("${RECOMP_ROOT}/clownmdemu-core" clownmdemu-core)

set(SDL2_BUNDLED "${RUNNER_ROOT}/external/SDL2/SDL2-2.28.5")
# SDL2 discovery. Windows pins the bundled VC devel package: without
# NO_DEFAULT_PATH, an MSYS2 SDL2 (C:/msys64/mingw64/lib/cmake/SDL2) can win and
# leak msys64 stdio.h onto cl.exe's include path, breaking the MSVC build.
# macOS/Linux use the system/Homebrew SDL2 (Apple Silicon: /opt/homebrew).
if(WIN32)
    set(SDL2_DIR "${SDL2_BUNDLED}/cmake" CACHE PATH "" FORCE)
    find_package(SDL2 REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${SDL2_DIR}")
else()
    if(APPLE)
        find_program(BREW_EXECUTABLE brew)
        if(BREW_EXECUTABLE)
            execute_process(COMMAND "${BREW_EXECUTABLE}" --prefix sdl2
                OUTPUT_VARIABLE _brew_sdl2_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE _brew_sdl2_rc)
            if(_brew_sdl2_rc EQUAL 0 AND _brew_sdl2_prefix)
                list(APPEND CMAKE_PREFIX_PATH "${_brew_sdl2_prefix}")
            endif()
        endif()
    endif()
    find_package(SDL2 REQUIRED)
endif()

# The runner includes SDL as <SDL2/SDL.h> (the bundled Windows devel layout).
# Homebrew's SDL2::SDL2 points the include dir *at* .../include/SDL2, so add
# its parent so <SDL2/SDL.h> resolves the same way on every platform.
set(SDL2_PARENT_INCLUDE_DIRS "")
foreach(_inc IN LISTS SDL2_INCLUDE_DIRS)
    get_filename_component(_inc_name "${_inc}" NAME)
    if(_inc_name STREQUAL "SDL2")
        get_filename_component(_inc_parent "${_inc}" DIRECTORY)
        list(APPEND SDL2_PARENT_INCLUDE_DIRS "${_inc_parent}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# YM2612 FM = vendored ymfm (Aaron Giles, BSD-3) behind the stable ym2612_*
# API (runner/audio/ym2612.h). Replaced clownmdemu's FM (AGPL) 2026-06-05 as
# part of the AGPL-free runtime initiative — see segagenesisrecomp/LICENSING.md
# (teardown item #2). enable_language(CXX) because ymfm is C++.
# ---------------------------------------------------------------------------
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(FM_SOURCES
    "${RUNNER_ROOT}/audio/ym2612_ymfm.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_opn.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_ssg.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_adpcm.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_pcm.cpp"
)

# ---------------------------------------------------------------------------
# Source set used by both the native and the oracle target (everything
# except the variant-specific files: stub_clown68000.c for native and
# the hybrid/oracle interpreter files for oracle).
# ---------------------------------------------------------------------------

set(SONIC2_SHARED_SOURCES
    "${RUNNER_ROOT}/main.c"
    "${RUNNER_ROOT}/audio.c"
    "${RUNNER_ROOT}/cmd_server.c"
    "${RUNNER_ROOT}/frame_snapshots.c"
    "${RUNNER_ROOT}/hybrid_global.c"
    "${RUNNER_ROOT}/glue.c"
    "${RUNNER_ROOT}/fiber_compat.c"   # cross-platform fiber shim (Win32 Fibers / POSIX ucontext) — glue.c depends on it
    "${RUNNER_ROOT}/crash_report.c"
    "${RUNNER_ROOT}/input_script.c"
    "${RUNNER_ROOT}/gamepad.c"
    "${RUNNER_ROOT}/chip_trace.c"
    "${RUNNER_ROOT}/audio/event_queue.c"
    ${FM_SOURCES}
    "${RUNNER_ROOT}/audio/sn76489.c"
    "${RUNNER_ROOT}/audio/mixer.c"
    "${RUNNER_ROOT}/audio/observability.c"
    # Verified-enhancement shadow layer (opt-in via env, default OFF; links no
    # clownmdemu, safe to ship). See segagenesisrecomp/docs/SHADOW_ENHANCEMENTS.md.
    "${RUNNER_ROOT}/audio/audio_shadow.c"   # envelope-correlation verifier
    "${RUNNER_ROOT}/audio/fm_shadow.cpp"    # parallel ymfm chip (cleaner highs / ym3438 ladder-off)
    "${RUNNER_ROOT}/video/color_lut.c"      # present-time 9-bit CRT/composite LUT
    "${RECOMP_ROOT}/sonicthehedgehog2/generated/sonic2_full.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/generated/sonic2_dispatch.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/generated/sonic2_layout.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_spec.c"
)

if(SONIC_REVERSE_DEBUG)
    list(APPEND SONIC2_SHARED_SOURCES "${RUNNER_ROOT}/reverse_debug.c")
    message(STATUS "SONIC_REVERSE_DEBUG=ON — Tier-1 reverse-debug instrumentation compiled in")
else()
    message(STATUS "SONIC_REVERSE_DEBUG=OFF — stripped native build (no rdb ring)")
endif()

# Include paths. THE NATIVE LIST MUST CONTAIN NO clownmdemu-core PATHS —
# that is the enforcement that no AGPL header can be compiled into a release
# binary (clowncommon is ISC-permissive and vendored at
# runner/external/clowncommon). The oracle keeps the full clownmdemu paths;
# it is the AGPL parity reference and never shipped.
set(SONIC2_INCLUDES_NATIVE
    "${RUNNER_ROOT}"
    "${RUNNER_ROOT}/include"
    "${RUNNER_ROOT}/video"
    "${RUNNER_ROOT}/external/ymfm/src"
    "${RUNNER_ROOT}/external/superzazu"
    "${RUNNER_ROOT}/external/clowncommon"
    "${RECOMP_ROOT}/sonicthehedgehog2"
)
set(SONIC2_INCLUDES_ORACLE
    "${RUNNER_ROOT}"
    "${RUNNER_ROOT}/include"
    "${RUNNER_ROOT}/video"
    "${RUNNER_ROOT}/external/ymfm/src"
    "${RUNNER_ROOT}/external/superzazu"
    "${RECOMP_ROOT}/clownmdemu-core/libraries/clownz80/source"
    "${RECOMP_ROOT}/sonicthehedgehog2"
    "${RECOMP_ROOT}/clownmdemu-core/source"
    "${RECOMP_ROOT}/clownmdemu-core/libraries/clowncommon"
    "${RECOMP_ROOT}/clownmdemu-core/libraries/clown68000/source/interpreter"
    ${SDL2_PARENT_INCLUDE_DIRS}
)

# Clean-room own backend (VDP+bus+Z80 scheduler, superzazu Z80 core embedded
# directly) — native target only; this is the AGPL-free Sonic 2 runtime.
# Oracle keeps clownmdemu (incl. its own clownz80) as ground truth.
set(OWN_BACKEND_SOURCES
    "${RUNNER_ROOT}/video/genesis_vdp.c"
    "${RUNNER_ROOT}/video/genesis_bus.c"
    "${RUNNER_ROOT}/video/genesis_machine.c"
    "${RUNNER_ROOT}/external/superzazu/z80.c")

set(SONIC2_LIBS SDL2::SDL2 SDL2::SDL2main)   # clownmdemu-core linked by oracle only
if(WIN32)
    list(APPEND SONIC2_LIBS ws2_32 comdlg32)
endif()

# Helper: copy SDL2.dll, ROM, and annotations CSV next to a target's exe.
# `includes` selects the per-target include list (native = no clown paths).
function(_sonic2_finalize_target tgt includes)
    target_include_directories(${tgt} PRIVATE ${${includes}})
    target_link_libraries(${tgt} PRIVATE ${SONIC2_LIBS})
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W3 /WX- /utf-8 /wd4102 /wd4996)
        target_link_options(${tgt} PRIVATE /MAP)
    endif()
    if(WIN32)
        # SDL2 is a DLL on Windows; macOS/Linux link the shared lib via rpath.
        add_custom_command(TARGET ${tgt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_BUNDLED}/lib/x64/SDL2.dll"
                "$<TARGET_FILE_DIR:${tgt}>"
            COMMENT "Copying SDL2.dll for ${tgt}")
        add_custom_command(TARGET ${tgt} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RECOMP_ROOT}/sonicthehedgehog2/sonic2.bin"
                "$<TARGET_FILE_DIR:${tgt}>/sonic2.bin"
            COMMENT "Copying Sonic 2 ROM for ${tgt}")
    endif()
    # Annotation CSV used by crash_report.c for symbol resolution (all platforms).
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RECOMP_ROOT}/sonicthehedgehog2/annotations_from_disasm.csv"
            "$<TARGET_FILE_DIR:${tgt}>/annotations_from_disasm.csv"
        COMMENT "Copying Sonic 2 annotations for ${tgt}")
endfunction()

# ---------------------------------------------------------------------------
# Native target — recompiled 68K drives the game (stub_clown68000 absorbs
# the would-be interpreter calls).
# ---------------------------------------------------------------------------

add_executable(SonicTheHedgehog2Recomp
    ${SONIC2_SHARED_SOURCES}
    ${OWN_BACKEND_SOURCES}          # clean-room own backend (native = own, AGPL-free)
)
target_compile_definitions(SonicTheHedgehog2Recomp PRIVATE
    INCLUDE_GENERATED_CODE=1
    ENABLE_RECOMPILED_CODE=1
    OWN_BACKEND=1                   # the own backend IS the Sonic 2 runtime (no clownmdemu link)
    DEFAULT_DEBUG_PORT=4380
    $<$<BOOL:${SONIC_REVERSE_DEBUG}>:SONIC_REVERSE_DEBUG=1>
    $<$<BOOL:${GEN_DEV_TRACE}>:GEN_DEV_TRACE>
)
_sonic2_finalize_target(SonicTheHedgehog2Recomp SONIC2_INCLUDES_NATIVE)

# ---------------------------------------------------------------------------
# Oracle target — clown68000 interpreter executes everything. Used as the
# reference for divergence-diff against the native target. Sonic 2's
# hybrid table is empty (sonic2_hybrid_table.c) so every recompiled
# function falls through to interpreter execution.
# ---------------------------------------------------------------------------

set(SONIC2_ORACLE_EXTRA_SOURCES
    "${RUNNER_ROOT}/hybrid.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_hybrid_table.c"
    "${RUNNER_ROOT}/hybrid_jmp.c"
)
if(SONIC_REVERSE_DEBUG)
    # Tier 3: per-instruction capture on the oracle side. Oracle-only because
    # native uses stub_clown68000.c and never runs the interpreter.
    list(APPEND SONIC2_ORACLE_EXTRA_SOURCES "${RUNNER_ROOT}/oracle_trace.c")
endif()

add_executable(SonicTheHedgehog2Recomp_oracle
    ${SONIC2_SHARED_SOURCES}
    ${SONIC2_ORACLE_EXTRA_SOURCES}
)
target_compile_definitions(SonicTheHedgehog2Recomp_oracle PRIVATE
    INCLUDE_GENERATED_CODE=1
    HYBRID_RECOMPILED_CODE=1
    SONIC_ORACLE_BUILD=1
    DEFAULT_DEBUG_PORT=4381
    $<$<BOOL:${SONIC_REVERSE_DEBUG}>:SONIC_REVERSE_DEBUG=1>
    $<$<BOOL:${GEN_DEV_TRACE}>:GEN_DEV_TRACE>
)
_sonic2_finalize_target(SonicTheHedgehog2Recomp_oracle SONIC2_INCLUDES_ORACLE)
# Oracle (clown68000 interpreter + clownmdemu VDP/bus) — the only target that
# links the AGPL core. Dev-only ground truth; never shipped.
target_link_libraries(SonicTheHedgehog2Recomp_oracle PRIVATE clownmdemu-core)
