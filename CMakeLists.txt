cmake_minimum_required(VERSION 3.16)
project(SonicTheHedgehog2Recomp VERSION 0.6.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(SONIC2_GAME_VERSION "0.6.1" CACHE STRING
    "Version used to match compatible Sonic 2 netplay lobbies")

# ---------------------------------------------------------------------------
# Sonic the Hedgehog 2 native build — sibling to SonicTheHedgehogRecomp/.
#
# All shared engine code (runner) lives in the canonical segagenesisrecomp
# checkout at the workspace root, reached here through a local directory
# junction (./segagenesisrecomp -> ../segagenesisrecomp). Per-game files
# (sonic2_spec.c) live alongside the Sonic 2 generated
# TUs in segagenesisrecomp/sonicthehedgehog2/. Sonic 2 does not consume any
# files from Sonic 1 directly.
# ---------------------------------------------------------------------------

# Engine root. Public layout: the committed `segagenesisrecomp` submodule
# (clone --recursive is self-contained). Local-dev override: if a gitignored
# `engine-local` symlink -> ../segagenesisrecomp is present, prefer it so every
# game repo shares ONE engine checkout. CI and audit builds can instead pass an
# explicit checkout without mutating links.
set(GENESIS_RECOMP_ROOT "" CACHE PATH
    "Optional shared Genesis framework checkout (defaults to engine-local/submodule)")
if(GENESIS_RECOMP_ROOT)
    set(RECOMP_ROOT "${GENESIS_RECOMP_ROOT}")
    message(STATUS "engine: explicit GENESIS_RECOMP_ROOT override")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/engine-local")
    set(RECOMP_ROOT "${CMAKE_SOURCE_DIR}/engine-local")
    message(STATUS "engine: local-dev override (engine-local/ -> shared checkout)")
else()
    set(RECOMP_ROOT "${CMAKE_SOURCE_DIR}/segagenesisrecomp")
    message(STATUS "engine: committed segagenesisrecomp/ submodule")
endif()
set(RUNNER_ROOT "${RECOMP_ROOT}/runner")

# Machine quickstates contain private C layouts and translated CPU state.
# Reconfigure on source changes and reject states from incompatible builds;
# campaign SRAM remains portable and independent of this fingerprint.
file(GLOB_RECURSE _state_inputs CONFIGURE_DEPENDS
    "${RUNNER_ROOT}/*.c" "${RUNNER_ROOT}/*.cpp" "${RUNNER_ROOT}/*.h"
    "${RECOMP_ROOT}/sonicthehedgehog2/*.c" "${RECOMP_ROOT}/sonicthehedgehog2/*.h"
    "${RECOMP_ROOT}/sonicthehedgehog2/*.toml"
    "${RECOMP_ROOT}/external/m68k-recomp-core/*.c" "${RECOMP_ROOT}/external/m68k-recomp-core/*.h"
    "${RECOMP_ROOT}/external/z80-recomp-core/*.c" "${RECOMP_ROOT}/external/z80-recomp-core/*.h"
    "${RECOMP_ROOT}/recompiler/src/*.c" "${RECOMP_ROOT}/recompiler/src/*.h")
list(FILTER _state_inputs EXCLUDE REGEX "/(generated|s2disasm|external/SDL2)/")
list(SORT _state_inputs)
set(_state_fingerprint "Sonic2-state-v1;${CMAKE_C_COMPILER_ID};${CMAKE_C_COMPILER_VERSION};${CMAKE_SIZEOF_VOID_P};${CMAKE_SYSTEM_PROCESSOR}")
foreach(_input IN LISTS _state_inputs)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_input}")
    file(SHA256 "${_input}" _hash)
    string(APPEND _state_fingerprint ";${_hash}")
endforeach()
include_directories("${CMAKE_BINARY_DIR}")

# Tier-1 reverse-debugger instrumentation. ON by default for
# day-to-day Sonic 2 bring-up (the rdb_* TCP commands / WRAM-write ring depend
# on it). Build a stripped shipping binary with -DSONIC_REVERSE_DEBUG=OFF
# (package_release.py does this).
option(SONIC_REVERSE_DEBUG
    "Enable Tier-1 reverse-debugger hooks (WRAM-write ring, rdb_* TCP cmds) and \
the oracle's Tier-3 per-instruction capture. OFF = stripped native build."
    ON)
option(GEN_DEV_TRACE
    "Compile the dev chip/snd trace rings + dumps (NEVER in release builds)" OFF)

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
# Source set for the native target (the only target besides the dev-only
# cosim build).
# ---------------------------------------------------------------------------

include("${RECOMP_ROOT}/cmake/GenesisRecompGenerated.cmake")
genesisrecomp_add_generated_sources(
    SONIC2_GENERATED_SOURCES sonic2 sonicthehedgehog2 sonic2.bin
    REVERSE_DEBUG "${SONIC_REVERSE_DEBUG}")

# Prod-vs-debug TCP strip (see SonicTheHedgehogRecomp/CMakeLists.txt). ON keeps
# the cmd_server TCP listener + frame_snapshots ring; OFF (release) swaps the
# no-op stub and drops frame_snapshots so the shipped binary opens no port.
option(GEN_ENABLE_TRACE
    "Build the TCP cmd server + observability rings (frame_snapshots). OFF for release." ON)
string(SHA256 _state_id "${_state_fingerprint};${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION};${SONIC_REVERSE_DEBUG};${GEN_DEV_TRACE};${GEN_ENABLE_TRACE};${CMAKE_C_FLAGS};${CMAKE_CXX_FLAGS};${CMAKE_C_FLAGS_RELEASE};${CMAKE_CXX_FLAGS_RELEASE}")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sonic2_state_build.h"
    CONTENT "#define SONIC2_STATE_BUILD_ID \"${_state_id}\"\n")
if(GEN_ENABLE_TRACE)
    set(GEN_TRACE_SOURCES
        "${RUNNER_ROOT}/cmd_server.c"
        "${RUNNER_ROOT}/frame_snapshots.c")
    add_compile_definitions(GEN_ENABLE_TRACE=1)
else()
    set(GEN_TRACE_SOURCES "${RUNNER_ROOT}/cmd_server_stub.c")
endif()

set(SONIC2_SHARED_SOURCES
    "${RUNNER_ROOT}/main.c"
    "${RUNNER_ROOT}/audio.c"
    ${GEN_TRACE_SOURCES}
    "${RUNNER_ROOT}/glue.c"
    "${RUNNER_ROOT}/fiber_compat.c"   # cross-platform fiber shim (Win32 Fibers / POSIX ucontext) — glue.c depends on it
    "${RUNNER_ROOT}/crash_report.c"
    "${RUNNER_ROOT}/input_script.c"
    "${RUNNER_ROOT}/gamepad.c"
    "${RUNNER_ROOT}/input_map.c"      # per-player Genesis button bindings (kbd+pad, 3/6-btn)
    "${RUNNER_ROOT}/app_config.c"     # settings.ini + rom.cfg load/save
    "${RUNNER_ROOT}/chip_trace.c"
    "${RUNNER_ROOT}/audio/event_queue.c"
    ${FM_SOURCES}
    "${RUNNER_ROOT}/audio/sn76489.c"
    "${RUNNER_ROOT}/audio/mixer.c"
    "${RUNNER_ROOT}/audio/observability.c"
    "${RUNNER_ROOT}/cosim_state.c"  # pointer-free end-state hash for benchmarks + co-sim
    # Verified-enhancement shadow layer (opt-in via env, default OFF; links no
    # clownmdemu, safe to ship). See segagenesisrecomp/docs/SHADOW_ENHANCEMENTS.md.
    "${RUNNER_ROOT}/audio/audio_shadow.c"   # envelope-correlation verifier
    "${RUNNER_ROOT}/audio/fm_shadow.cpp"    # parallel ymfm chip (cleaner highs / ym3438 ladder-off)
    "${RUNNER_ROOT}/video/color_lut.c"      # present-time 9-bit CRT/composite LUT
    ${SONIC2_GENERATED_SOURCES}
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_spec.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_video.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_party.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_options.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_donor_assets.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_resources.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_save_assets.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_save_draw.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_save_menu.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_campaign_file.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_campaign.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_campaign_store.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_mods.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_runtime.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_character.c"
)

if(SONIC_REVERSE_DEBUG)
    list(APPEND SONIC2_SHARED_SOURCES "${RUNNER_ROOT}/reverse_debug.c")
    message(STATUS "SONIC_REVERSE_DEBUG=ON — Tier-1 reverse-debug instrumentation compiled in")
else()
    message(STATUS "SONIC_REVERSE_DEBUG=OFF — stripped native build (no rdb ring)")
endif()

# Include paths. Nothing third-party here beyond ymfm/superzazu/clowncommon —
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
    "${CMAKE_SOURCE_DIR}/recomp-ui/src/common"
)

# Clean-room own backend (VDP+bus+Z80 scheduler, superzazu Z80 core embedded
# directly) — native target only; this is the AGPL-free Sonic 2 runtime.
# Oracle keeps clownmdemu (incl. its own clownz80) as ground truth.
set(OWN_BACKEND_SOURCES
    "${RUNNER_ROOT}/video/genesis_vdp.c"
    "${RUNNER_ROOT}/video/genesis_bus.c"
    "${RUNNER_ROOT}/video/genesis_machine.c"
    "${RUNNER_ROOT}/external/superzazu/z80.c")

# Clean-room 68000 interpreter + decoder + rom_parser. The _wt-cosim engine's
# shared glue.c references m68k_interp_run_framed (the Tier-3 native floor)
# unconditionally, so EVERY target built against this engine must link these
# (AGPL-free — own interpreter + recompiler decoder). Sonic 2's original list
# omitted them because it targeted an older engine.
set(SONIC2_FLOOR_SOURCES
    "${RUNNER_ROOT}/m68k_interp.c"
    "${RECOMP_ROOT}/recompiler/src/m68k_decoder.c"
    "${RECOMP_ROOT}/recompiler/src/rom_parser.c")
list(APPEND SONIC2_INCLUDES_NATIVE "${RECOMP_ROOT}/recompiler/src")   # m68k_decoder.h

set(SONIC2_LIBS SDL2::SDL2main SDL2::SDL2)
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
    ${SONIC2_FLOOR_SOURCES}         # m68k_interp + decoder (this engine's glue.c needs it)
)
add_dependencies(SonicTheHedgehog2Recomp ${SONIC2_GENERATED_SOURCES_TARGET})
target_compile_definitions(SonicTheHedgehog2Recomp PRIVATE
    INCLUDE_GENERATED_CODE=1
    ENABLE_RECOMPILED_CODE=1
    OWN_BACKEND=1                   # the own backend IS the Sonic 2 runtime (no clownmdemu link)
    DEFAULT_DEBUG_PORT=4380
    $<$<BOOL:${SONIC_REVERSE_DEBUG}>:SONIC_REVERSE_DEBUG=1>
    $<$<BOOL:${GEN_DEV_TRACE}>:GEN_DEV_TRACE>
)
_sonic2_finalize_target(SonicTheHedgehog2Recomp SONIC2_INCLUDES_NATIVE)

# Shared recomp-ui (Dear ImGui) pre-boot launcher — native (shipped) target only
# Defines RECOMP_LAUNCHER (un-gates the seam in
# the runner's main.c), links system GL, stages assets/ next to the exe.
# The dev-only oracle target remains launcher-free.
set(RECOMP_UI_ENABLE_MODS ON CACHE BOOL "Expose Sonic 2's opt-in built-in mods")
include(${CMAKE_SOURCE_DIR}/recomp-ui/recomp_ui.cmake)
recomp_target_launcher_ui(SonicTheHedgehog2Recomp CONSOLE genesis
    BOXART ${CMAKE_SOURCE_DIR}/boxart.tga)

# Two-player delay-synchronized netplay. The engine owns the transport,
# lobby, and deterministic frame gate; recomp-ui supplies the waiting-room UI.
# ICE is enabled for Internet sessions while direct/LAN sessions remain
# available through the same launcher flow.
include(${RECOMP_ROOT}/cmake/GenesisRecompNetplay.cmake)
genesisrecomp_enable_netplay(SonicTheHedgehog2Recomp
    GAME_VERSION "${SONIC2_GAME_VERSION}"
    ICE
    PEER_VIEW)

option(BUILD_TESTING "Build ROM-independent renderer and framework tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory("${RECOMP_ROOT}/tests" "${CMAKE_BINARY_DIR}/tests")
endif()

# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Differential co-simulation targets (dev-only, never shipped). Mirror of
# Sonic 1's block (see _wt-cosim-s1/CMakeLists.txt + engine COSIM.md), adapted
# for Sonic 2. The shared glue.c references m68k_interp_run_framed and (under
# GENESIS_COSIM) FORCE_INTERP, so the cosim targets must link the clean-room
# 68000 interpreter + decoder + rom_parser, which Sonic 2's SHARED list omits.
# ---------------------------------------------------------------------------
option(GENESIS_BUILD_COSIM "Build the differential co-simulation target (dev-only)." OFF)
if(GENESIS_BUILD_COSIM)
    set(SONIC2_COSIM_FLOOR_SOURCES
        "${RUNNER_ROOT}/m68k_interp.c"
        "${RECOMP_ROOT}/recompiler/src/m68k_decoder.c"
        "${RECOMP_ROOT}/recompiler/src/rom_parser.c")

    add_executable(SonicTheHedgehog2Recomp_cosim
        "${CMAKE_SOURCE_DIR}/recomp-ui/src/common/sha256.c"
        ${SONIC2_SHARED_SOURCES}
        ${OWN_BACKEND_SOURCES}
        ${SONIC2_COSIM_FLOOR_SOURCES}
        "${RUNNER_ROOT}/cosim.c"
        "${RUNNER_ROOT}/cosim_visible.c"
        "${RUNNER_ROOT}/cosim_cycles.c"                             # cross-backend work-cycle ruler
        "${RUNNER_ROOT}/game_cycles.c"
        "${SONIC2_GENERATED_SOURCES_CYCLES}"                 # clown-measured costs
    )
    add_dependencies(SonicTheHedgehog2Recomp_cosim ${SONIC2_GENERATED_SOURCES_TARGET})
    target_compile_definitions(SonicTheHedgehog2Recomp_cosim PRIVATE
        INCLUDE_GENERATED_CODE=1
        ENABLE_RECOMPILED_CODE=1
        OWN_BACKEND=1
        GENESIS_COSIM=1
        DEFAULT_DEBUG_PORT=4394
        $<$<BOOL:${SONIC_REVERSE_DEBUG}>:SONIC_REVERSE_DEBUG=1>
    )
    _sonic2_finalize_target(SonicTheHedgehog2Recomp_cosim SONIC2_INCLUDES_NATIVE)
    target_include_directories(SonicTheHedgehog2Recomp_cosim PRIVATE
        "${RECOMP_ROOT}/recompiler/src")   # m68k_decoder.h / rom_parser.h
    message(STATUS "GENESIS_BUILD_COSIM=ON — Sonic 2 differential co-sim target enabled")

endif()
