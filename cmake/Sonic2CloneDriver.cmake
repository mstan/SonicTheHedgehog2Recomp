# Optional, build-bound sound image. No upstream music/sample data is in Git.
set(SONIC_CLONE_DRIVER_PROBE_DIR "" CACHE PATH "Prebuilt software-only input from tools/clone_driver_probe/build.py --rom-assets")
set(SONIC_CLONE_DRIVER_SUPPORT "AUTO" CACHE STRING "Build Clone Driver software: AUTO, ON or OFF (player mod remains disabled)")
set_property(CACHE SONIC_CLONE_DRIVER_SUPPORT PROPERTY STRINGS AUTO ON OFF)
set(SONIC_CLONE_DRIVER_ASSEMBLER "" CACHE FILEPATH "Optional native Macroassembler AS for this build host")
set(SONIC_CLONE_DRIVER_P2BIN "" CACHE FILEPATH "Optional native p2bin for this build host")
if((SONIC_CLONE_DRIVER_ASSEMBLER AND NOT SONIC_CLONE_DRIVER_P2BIN) OR
   (SONIC_CLONE_DRIVER_P2BIN AND NOT SONIC_CLONE_DRIVER_ASSEMBLER))
    message(FATAL_ERROR "Supply SONIC_CLONE_DRIVER_ASSEMBLER and SONIC_CLONE_DRIVER_P2BIN together")
endif()
if(NOT SONIC_CLONE_DRIVER_SUPPORT MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "SONIC_CLONE_DRIVER_SUPPORT must be AUTO, ON or OFF")
endif()
if(NOT SONIC_CLONE_DRIVER_PROBE_DIR AND NOT SONIC_CLONE_DRIVER_SUPPORT STREQUAL "OFF")
    set(_s2_clone_host_supported OFF)
    if((CMAKE_HOST_WIN32 OR CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux") AND
       CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$")
        set(_s2_clone_host_supported ON)
    endif()
    if(SONIC_CLONE_DRIVER_ASSEMBLER AND SONIC_CLONE_DRIVER_P2BIN)
        set(_s2_clone_host_supported ON)
    endif()
    if(_s2_clone_host_supported)
        find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
        set(_s2_clone_tools)
        if(SONIC_CLONE_DRIVER_ASSEMBLER)
            list(APPEND _s2_clone_tools --assembler "${SONIC_CLONE_DRIVER_ASSEMBLER}" --p2bin "${SONIC_CLONE_DRIVER_P2BIN}")
        endif()
        execute_process(COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/fetch_build.py"
            --cache "${CMAKE_BINARY_DIR}/clone-driver"
            --cmake-out "${CMAKE_BINARY_DIR}/clone-driver/probe-path.cmake"
            ${_s2_clone_tools} RESULT_VARIABLE _s2_clone_status)
        if(NOT _s2_clone_status EQUAL 0)
            if(SONIC_CLONE_DRIVER_SUPPORT STREQUAL "ON")
                message(FATAL_ERROR "Could not prepare pinned Clone Driver software; see the error above (or configure SONIC_CLONE_DRIVER_SUPPORT=OFF)")
            else()
                message(WARNING "Clone Driver preparation failed; AUTO is building the normal game without this optional driver. Use SUPPORT=ON to require it.")
            endif()
        else()
            include("${CMAKE_BINARY_DIR}/clone-driver/probe-path.cmake")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/fetch_build.py"
            "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/build.py"
            "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/driver_rom.asm"
            "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/sources.lock.json")
    elseif(SONIC_CLONE_DRIVER_SUPPORT STREQUAL "ON")
        message(FATAL_ERROR "Supply native SONIC_CLONE_DRIVER_ASSEMBLER and SONIC_CLONE_DRIVER_P2BIN on this build host")
    else()
        message(STATUS "Clone Driver unavailable on this build host without native AS/p2bin; building the normal game")
    endif()
endif()
if(SONIC_CLONE_DRIVER_PROBE_DIR AND NOT SONIC_CLONE_DRIVER_SUPPORT STREQUAL "OFF")
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    set(SONIC_GENERATION_GAME "${CMAKE_BINARY_DIR}/clone-input")
    file(GLOB _s2_clone_configs CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/game/*.toml" "${CMAKE_SOURCE_DIR}/game/*.csv")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${_s2_clone_configs} "${CMAKE_SOURCE_DIR}/game/sonic2.bin"
        "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/prepare_native.py"
        "${SONIC_CLONE_DRIVER_PROBE_DIR}/probe.json"
        "${SONIC_CLONE_DRIVER_PROBE_DIR}/driver.lst"
        "${SONIC_CLONE_DRIVER_PROBE_DIR}/clone-driver.bin")
    set(SONIC_MUSIC_DRIVER_PROGRAM_DIR "" CACHE PATH "Optional prebuilt software-only music sequencer")
    if(NOT SONIC_MUSIC_DRIVER_PROGRAM_DIR)
        file(SHA256 "${CMAKE_SOURCE_DIR}/tools/music_driver/build.py" _music_build_hash)
        file(SHA256 "${CMAKE_SOURCE_DIR}/tools/music_driver/driver.asm" _music_asm_hash)
        string(SHA256 _music_identity "${_music_build_hash};${_music_asm_hash}")
        string(SUBSTRING "${_music_identity}" 0 20 _music_identity)
        set(SONIC_MUSIC_DRIVER_PROGRAM_DIR "${CMAKE_BINARY_DIR}/music-driver/program-${_music_identity}")
        if(NOT EXISTS "${SONIC_MUSIC_DRIVER_PROGRAM_DIR}/program.json")
            execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tools/music_driver/build.py"
                --cache "${CMAKE_BINARY_DIR}/music-driver/cache" --output "${SONIC_MUSIC_DRIVER_PROGRAM_DIR}"
                ${_s2_clone_tools} COMMAND_ERROR_IS_FATAL ANY)
        endif()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/tools/music_driver/build.py" "${CMAKE_SOURCE_DIR}/tools/music_driver/driver.asm"
        "${SONIC_MUSIC_DRIVER_PROGRAM_DIR}/program.json" "${SONIC_MUSIC_DRIVER_PROGRAM_DIR}/clone-driver.bin")
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/prepare_native.py"
        --probe "${SONIC_CLONE_DRIVER_PROBE_DIR}" --game "${CMAKE_SOURCE_DIR}/game"
        --engine "${RECOMP_ROOT}" --output "${SONIC_GENERATION_GAME}"
        --music-program "${SONIC_MUSIC_DRIVER_PROGRAM_DIR}"
        COMMAND_ERROR_IS_FATAL ANY)

    # Keep the engine's generation rule pointed at the original ROM. The
    # build-time wrapper adds only the separately assembled driver programs.
    get_target_property(_s2_recompiler_sources GenesisRecomp SOURCES)
    list(FIND _s2_recompiler_sources "src/main_genesis.c" _s2_main_index)
    if(_s2_main_index EQUAL -1)
        message(FATAL_ERROR "The pinned recompiler entry point was not found")
    endif()
    list(REMOVE_AT _s2_recompiler_sources ${_s2_main_index})
    set_property(TARGET GenesisRecomp PROPERTY SOURCES "${_s2_recompiler_sources}")
    target_sources(GenesisRecomp PRIVATE
        "${CMAKE_SOURCE_DIR}/tools/clone_driver_probe/recompile_with_programs.c")
    target_include_directories(GenesisRecomp PRIVATE "${SONIC_GENERATION_GAME}")

    # Rebuild the generator when its prepared discovery files change. Its
    # software arrays are tracked through ordinary C header dependencies.
    file(GLOB _s2_prepared_configs LIST_DIRECTORIES false
        "${SONIC_GENERATION_GAME}/*.toml"
        "${SONIC_GENERATION_GAME}/*.csv")
    set(_s2_config_identity "")
    foreach(_s2_config IN LISTS _s2_prepared_configs)
        file(SHA256 "${_s2_config}" _s2_config_hash)
        string(APPEND _s2_config_identity "${_s2_config_hash}")
    endforeach()
    string(SHA256 _s2_config_identity "${_s2_config_identity}")
    file(GENERATE OUTPUT "${SONIC_GENERATION_GAME}/sonic2_clone_config.h"
        CONTENT "#define SONIC2_CLONE_CONFIG \"${SONIC_GENERATION_GAME}/game.toml\"\n/* ${_s2_config_identity} */\n")

    include_directories("${SONIC_GENERATION_GAME}")
    add_compile_definitions(SONIC2_CLONE_DRIVER=1)
endif()
