cmake_minimum_required(VERSION 3.16)
project(SonicTheHedgehog2Recomp C)

set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------------------------
# Sonic the Hedgehog 2 native build — sibling to SonicTheHedgehogRecomp/.
#
# The runner code lives in ../SonicTheHedgehogRecomp/runner/ and is
# game-agnostic now that consumers read everything per-game from
# g_game_spec (defined here in sonic2_spec.c). The generated 68K
# translation unit comes from segagenesisrecomp/sonicthehedgehog2/.
#
# Oracle build is deferred — Sonic 2 has no hybrid native-override
# table, and runner/hybrid_table.c is still hard-coded to Sonic 1's
# function set. That moves into per-game spec files at a later step.
# ---------------------------------------------------------------------------

set(SHARED_REPO_ROOT "${CMAKE_SOURCE_DIR}/../SonicTheHedgehogRecomp")
set(RUNNER_ROOT      "${SHARED_REPO_ROOT}/runner")
set(RECOMP_ROOT      "${SHARED_REPO_ROOT}/segagenesisrecomp")

include("${RECOMP_ROOT}/runner/clownmdemu_patches.cmake")
add_subdirectory("${RECOMP_ROOT}/clownmdemu-core" clownmdemu-core)

set(SDL2_BUNDLED "${RECOMP_ROOT}/runner/external/SDL2/SDL2-2.28.5")
# Pin SDL2 to the bundled copy. Without NO_DEFAULT_PATH, msys64's
# SDL2 (under C:/msys64/mingw64/lib/cmake/SDL2) wins on systems with
# MSYS2 installed, which then leaks msys64 stdio.h onto cl.exe's
# include search path and breaks the MSVC build.
set(SDL2_DIR "${SDL2_BUNDLED}/cmake" CACHE PATH "" FORCE)
find_package(SDL2 REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${SDL2_DIR}")

# ---------------------------------------------------------------------------
# Source list — every runner file we need EXCEPT the Sonic-1-specific ones:
#   sonic_extras.c    — Sonic-1 RAM offsets + sonic_state/object_table cmds
#   sonic1_spec.c     — Sonic-1 g_game_spec
#   hybrid_table.c    — Sonic-1 verified-clean native overrides
#   hybrid.c, hybrid_jmp.c — oracle-only, not built for Sonic 2
#   reverse_debug.c   — opt-in, not enabled here
# ---------------------------------------------------------------------------

set(SONIC2_SOURCES
    "${RUNNER_ROOT}/main.c"
    "${RUNNER_ROOT}/audio.c"
    "${RUNNER_ROOT}/cmd_server.c"
    "${RUNNER_ROOT}/frame_snapshots.c"
    "${RUNNER_ROOT}/hybrid_global.c"
    "${RUNNER_ROOT}/glue.c"
    "${RUNNER_ROOT}/crash_report.c"
    "${RUNNER_ROOT}/reverse_debug.c"
    "${RUNNER_ROOT}/audio/event_queue.c"
    "${RUNNER_ROOT}/audio/ym2612.c"
    "${RUNNER_ROOT}/audio/sn76489.c"
    "${RUNNER_ROOT}/audio/mixer.c"
    "${RUNNER_ROOT}/audio/observability.c"
    "${RUNNER_ROOT}/stub_clown68000.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/generated/sonic2_full.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/generated/sonic2_dispatch.c"
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_spec.c"
)

set(SONIC2_INCLUDES
    "${RUNNER_ROOT}"
    "${RECOMP_ROOT}/runner/include"
    "${RECOMP_ROOT}/sonicthehedgehog2"
    "${RECOMP_ROOT}/clownmdemu-core/source"
    "${RECOMP_ROOT}/clownmdemu-core/libraries/clowncommon"
    "${RECOMP_ROOT}/clownmdemu-core/libraries/clown68000/source/interpreter"
)

set(SONIC2_LIBS clownmdemu-core SDL2::SDL2 SDL2::SDL2main)
if(WIN32)
    list(APPEND SONIC2_LIBS ws2_32 comdlg32)
endif()

add_executable(SonicTheHedgehog2Recomp ${SONIC2_SOURCES})

target_compile_definitions(SonicTheHedgehog2Recomp PRIVATE
    INCLUDE_GENERATED_CODE=1
    ENABLE_RECOMPILED_CODE=1
    DEFAULT_DEBUG_PORT=4380
    SONIC_REVERSE_DEBUG=1
)

target_include_directories(SonicTheHedgehog2Recomp PRIVATE ${SONIC2_INCLUDES})
target_link_libraries(SonicTheHedgehog2Recomp PRIVATE ${SONIC2_LIBS})

if(MSVC)
    target_compile_options(SonicTheHedgehog2Recomp PRIVATE /W3 /WX- /utf-8 /wd4102 /wd4996)
    target_link_options(SonicTheHedgehog2Recomp PRIVATE /MAP)
endif()

if(WIN32)
    add_custom_command(TARGET SonicTheHedgehog2Recomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_BUNDLED}/lib/x64/SDL2.dll"
            "$<TARGET_FILE_DIR:SonicTheHedgehog2Recomp>"
        COMMENT "Copying SDL2.dll for SonicTheHedgehog2Recomp")

    # Also copy the Sonic 2 ROM into the build dir if present alongside
    # the cfg, so the binary can be launched from the build output dir
    # the same way SonicTheHedgehogRecomp expects sonic.bin.
    add_custom_command(TARGET SonicTheHedgehog2Recomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RECOMP_ROOT}/sonicthehedgehog2/sonic2.bin"
            "$<TARGET_FILE_DIR:SonicTheHedgehog2Recomp>/sonic2.bin"
        COMMENT "Copying Sonic 2 ROM for runtime")

    # Annotation CSV used by crash_report.c for symbol resolution.
    add_custom_command(TARGET SonicTheHedgehog2Recomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RECOMP_ROOT}/sonicthehedgehog2/annotations_from_disasm.csv"
            "$<TARGET_FILE_DIR:SonicTheHedgehog2Recomp>/annotations_from_disasm.csv"
        COMMENT "Copying Sonic 2 annotations for crash-report symbol resolution")
endif()
