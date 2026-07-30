cmake_minimum_required(VERSION 3.16)

project(Sonic2RecompAndroid C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ---------------------------------------------------------------------------
# Sonic 2 Android target — arm64 libmain.so consumed by org.libsdl.app.
#
# Mirrors the desktop CMakeLists source set (native target) with the
# desktop-only pieces dropped: no recomp-ui launcher, no netplay, no cosim,
# reverse-debug OFF. Generated C is NOT produced here — the host recompiler
# cannot run under an NDK cross-build, so android/generated/sonic2/ is
# emitted beforehand on the host (see android/README-android.md).
# ---------------------------------------------------------------------------

get_filename_component(ANDROID_PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
get_filename_component(GAME_REPO_ROOT    "${ANDROID_PROJ_ROOT}/.." ABSOLUTE)
set(RECOMP_ROOT   "${GAME_REPO_ROOT}/segagenesisrecomp")
set(RUNNER_ROOT   "${RECOMP_ROOT}/runner")
set(GEN_DIR       "${ANDROID_PROJ_ROOT}/generated/sonic2")
set(UCONTEXT_ROOT "${ANDROID_PROJ_ROOT}/external/libucontext")

if(NOT EXISTS "${GEN_DIR}/sonic2_dispatch.c")
    message(FATAL_ERROR "Pre-generated C missing at ${GEN_DIR} — run the host "
        "GenesisRecomp into that directory first (see android/README-android.md)")
endif()

file(GLOB SONIC2_GENERATED_SOURCES "${GEN_DIR}/sonic2_part*.c")
list(APPEND SONIC2_GENERATED_SOURCES
    "${GEN_DIR}/sonic2_dispatch.c"
    "${GEN_DIR}/sonic2_layout.c")

# bionic ships no makecontext/swapcontext — the vendored libucontext (ISC)
# provides them for fiber_compat.c's ucontext branch (aarch64 only).
set(UCONTEXT_SOURCES
    "${UCONTEXT_ROOT}/arch/aarch64/getcontext.S"
    "${UCONTEXT_ROOT}/arch/aarch64/setcontext.S"
    "${UCONTEXT_ROOT}/arch/aarch64/swapcontext.S"
    "${UCONTEXT_ROOT}/arch/aarch64/makecontext.c"
    "${UCONTEXT_ROOT}/arch/aarch64/trampoline.c")

set(FM_SOURCES
    "${RUNNER_ROOT}/audio/ym2612_ymfm.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_opn.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_ssg.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_adpcm.cpp"
    "${RUNNER_ROOT}/external/ymfm/src/ymfm_pcm.cpp")

add_library(main SHARED
    "${RUNNER_ROOT}/main.c"
    "${RUNNER_ROOT}/audio.c"
    "${RUNNER_ROOT}/cmd_server.c"
    "${RUNNER_ROOT}/frame_snapshots.c"
    "${RUNNER_ROOT}/glue.c"
    "${RUNNER_ROOT}/fiber_compat.c"
    "${RUNNER_ROOT}/crash_report.c"
    "${RUNNER_ROOT}/input_script.c"
    "${RUNNER_ROOT}/gamepad.c"
    "${RUNNER_ROOT}/input_map.c"
    "${RUNNER_ROOT}/app_config.c"
    "${RUNNER_ROOT}/chip_trace.c"
    "${RUNNER_ROOT}/audio/event_queue.c"
    ${FM_SOURCES}
    "${RUNNER_ROOT}/audio/sn76489.c"
    "${RUNNER_ROOT}/audio/mixer.c"
    "${RUNNER_ROOT}/audio/observability.c"
    "${RUNNER_ROOT}/audio/audio_shadow.c"
    "${RUNNER_ROOT}/audio/fm_shadow.cpp"
    "${RUNNER_ROOT}/video/color_lut.c"
    # clean-room own backend
    "${RUNNER_ROOT}/video/genesis_vdp.c"
    "${RUNNER_ROOT}/video/genesis_bus.c"
    "${RUNNER_ROOT}/video/genesis_machine.c"
    "${RUNNER_ROOT}/external/superzazu/z80.c"
    # Tier-3 floor (glue.c references m68k_interp_run_framed unconditionally)
    "${RUNNER_ROOT}/m68k_interp.c"
    "${RECOMP_ROOT}/recompiler/src/m68k_decoder.c"
    "${RECOMP_ROOT}/recompiler/src/rom_parser.c"
    ${SONIC2_GENERATED_SOURCES}
    "${RECOMP_ROOT}/sonicthehedgehog2/sonic2_spec.c"
    ${UCONTEXT_SOURCES})

target_compile_definitions(main PRIVATE
    INCLUDE_GENERATED_CODE=1
    ENABLE_RECOMPILED_CODE=1
    OWN_BACKEND=1
    GEN_ENABLE_TRACE=1
    DEFAULT_DEBUG_PORT=4380)

target_include_directories(main PRIVATE
    "${RUNNER_ROOT}"
    "${RUNNER_ROOT}/include"
    "${RUNNER_ROOT}/video"
    "${RUNNER_ROOT}/external/ymfm/src"
    "${RUNNER_ROOT}/external/superzazu"
    "${RUNNER_ROOT}/external/clowncommon"
    "${RECOMP_ROOT}/sonicthehedgehog2"
    "${RECOMP_ROOT}/recompiler/src"
    "${GEN_DIR}"
    # <SDL2/SDL.h> include style: shim dir whose SDL2/ symlinks SDL's include/
    "${CMAKE_CURRENT_SOURCE_DIR}/../shim-include"
    # libucontext: public header, aarch64 bits.h, defs.h + common-defs.h
    "${UCONTEXT_ROOT}/include"
    "${UCONTEXT_ROOT}/arch/aarch64/include"
    "${UCONTEXT_ROOT}/arch/aarch64"
    "${UCONTEXT_ROOT}/arch/common")

target_link_libraries(main SDL2 SDL2main log GLESv2)
