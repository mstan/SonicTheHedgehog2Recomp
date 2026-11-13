# Sonic 2 implementation tests belong to the game, never the shared engine.
set(S2_GAME "${CMAKE_CURRENT_LIST_DIR}/../game")
set(S2_TOOLS "${CMAKE_CURRENT_LIST_DIR}/../tools")

add_executable(sonic2_sound_assets_test runtime/sonic2_sound_assets_test.c
    "${S2_GAME}/sonic2_sound_assets.c" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common/sha256.c")
target_include_directories(sonic2_sound_assets_test PRIVATE "${S2_GAME}" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common")
target_compile_options(sonic2_sound_assets_test PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME sonic2_sound_assets COMMAND sonic2_sound_assets_test "${CMAKE_SOURCE_DIR}/game/sonic2.bin")
add_executable(sonic2_sound_driver_test runtime/sonic2_sound_driver_test.c
    "${S2_GAME}/sonic2_sound_driver.c" "${S2_GAME}/sonic2_sound_mods.c" "${S2_GAME}/sonic2_sound_assets.c"
    "${RUNNER_ROOT}/host_atomic_file.c" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common/sha256.c")
target_compile_definitions(sonic2_sound_driver_test PRIVATE RECOMP_LAUNCHER=1)
target_include_directories(sonic2_sound_driver_test PRIVATE "${S2_GAME}" "${RUNNER_ROOT}" "${RUNNER_ROOT}/include"
    "${RUNNER_ROOT}/external/superzazu" "${CMAKE_SOURCE_DIR}/recomp-ui/src" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common")
target_link_libraries(sonic2_sound_driver_test PRIVATE SDL2::SDL2)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sound-fixtures")
add_test(NAME sonic2_sound_driver COMMAND sonic2_sound_driver_test "${CMAKE_SOURCE_DIR}/game/sonic2.bin"
    "${CMAKE_CURRENT_BINARY_DIR}/sound-fixtures/settings.ini")
if(WIN32)
    add_custom_command(TARGET sonic2_sound_driver_test POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:sonic2_sound_driver_test>)
endif()

function(s2_test name)
    add_executable(${name}_test runtime/${name}_test.c ${ARGN})
    target_include_directories(${name}_test PRIVATE "${S2_GAME}" "${RUNNER_ROOT}" "${RUNNER_ROOT}/include")
    if(MSVC)
        target_compile_options(${name}_test PRIVATE /UNDEBUG)
    else()
        target_compile_options(${name}_test PRIVATE -UNDEBUG)
    endif()
    if(name STREQUAL "sonic2_resources")
        add_test(NAME ${name} COMMAND ${name}_test "${CMAKE_CURRENT_BINARY_DIR}/resource-test-absent.ini")
    else()
        add_test(NAME ${name} COMMAND ${name}_test)
    endif()
endfunction()

s2_test(sonic2_campaign "${S2_GAME}/sonic2_campaign.c" "${S2_GAME}/sonic2_campaign_store.c")
s2_test(sonic2_save_draw "${S2_GAME}/sonic2_campaign.c" "${S2_GAME}/sonic2_save_draw.c")
s2_test(sonic2_campaign_file "${S2_GAME}/sonic2_campaign.c" "${S2_GAME}/sonic2_campaign_store.c"
    "${S2_GAME}/sonic2_campaign_file.c" "${S2_GAME}/sonic2_party.c")
target_include_directories(sonic2_campaign_file_test PRIVATE "${RUNNER_ROOT}/external/superzazu")
s2_test(sonic2_party "${S2_GAME}/sonic2_party.c")
s2_test(sonic2_stage_layouts "${S2_GAME}/sonic2_stage_layouts.c")
s2_test(sonic2_options_netplay "${S2_GAME}/sonic2_party.c")
target_compile_definitions(sonic2_options_netplay_test PRIVATE GENESIS_HAS_RECOMP_NET=1)
target_include_directories(sonic2_options_netplay_test PRIVATE "${RUNNER_ROOT}/external/superzazu")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/party-fixtures")
set_tests_properties(sonic2_party PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/party-fixtures")
s2_test(sonic2_donor "${S2_GAME}/sonic2_donor_assets.c")
s2_test(sonic2_video "${RUNNER_ROOT}/video/genesis_vdp.c")
target_compile_definitions(sonic2_video_test PRIVATE GENESIS_HAS_RECOMP_NET=1 OWN_BACKEND=1)
target_include_directories(sonic2_video_test PRIVATE "${RUNNER_ROOT}/external/superzazu")
s2_test(sonic2_save_menu "${S2_GAME}/sonic2_campaign.c")
s2_test(sonic2_character "${S2_GAME}/sonic2_character.c" "${S2_GAME}/sonic2_shadow.c" "${S2_GAME}/sonic2_super.c")
s2_test(sonic2_super "${S2_GAME}/sonic2_super.c")
s2_test(sonic2_shadow "${S2_GAME}/sonic2_shadow.c" "${S2_GAME}/sonic2_character.c" "${S2_GAME}/sonic2_super.c")
s2_test(sonic2_mighty "${S2_GAME}/sonic2_mighty.c" "${S2_GAME}/sonic2_shadow_roster.c"
    "${S2_GAME}/sonic2_party.c" "${S2_GAME}/sonic2_super.c"
    "${S2_GAME}/sonic2_resources.c" "${S2_GAME}/sonic2_stage_layouts.c" "${S2_GAME}/sonic2_knuckles_ending.c" "${S2_GAME}/sonic2_donor_assets.c"
    "${S2_GAME}/sonic2_save_assets.c" "${S2_GAME}/sonic2_campaign.c"
    "${CMAKE_SOURCE_DIR}/recomp-ui/src/common/sha256.c")
target_include_directories(sonic2_mighty_test PRIVATE "${CMAKE_SOURCE_DIR}/recomp-ui/src/common")

if(EXISTS "${CMAKE_SOURCE_DIR}/recomp-ui/src/recomp_launcher.h")
    set(S2_SHA "${CMAKE_SOURCE_DIR}/recomp-ui/src/common/sha256.c")
    add_executable(sonic2_save_preview "${S2_TOOLS}/sonic2_save_preview.c"
        "${S2_GAME}/sonic2_campaign.c" "${S2_GAME}/sonic2_save_assets.c"
        "${S2_GAME}/sonic2_save_draw.c" "${S2_SHA}")
    add_executable(sonic2_donor_inspect "${S2_TOOLS}/sonic2_donor_inspect.c"
        "${S2_GAME}/sonic2_donor_assets.c" "${S2_SHA}")
    foreach(tool sonic2_save_preview sonic2_donor_inspect)
        target_include_directories(${tool} PRIVATE "${S2_GAME}" "${RUNNER_ROOT}"
            "${RUNNER_ROOT}/video" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common")
    endforeach()
    s2_test(sonic2_resources "${S2_GAME}/sonic2_party.c" "${S2_GAME}/sonic2_campaign.c"
        "${S2_GAME}/sonic2_campaign_store.c" "${S2_GAME}/sonic2_campaign_file.c"
        "${S2_GAME}/sonic2_resources.c" "${S2_GAME}/sonic2_stage_layouts.c" "${S2_GAME}/sonic2_knuckles_ending.c" "${S2_GAME}/sonic2_save_assets.c"
        "${S2_GAME}/sonic2_donor_assets.c" "${S2_GAME}/sonic2_mods.c" "${S2_SHA}")
    target_compile_definitions(sonic2_resources_test PRIVATE RECOMP_LAUNCHER=1 GENESIS_HAS_RECOMP_NET=1)
    target_include_directories(sonic2_resources_test PRIVATE
        "${CMAKE_SOURCE_DIR}/recomp-ui/src" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common"
        "${RUNNER_ROOT}/external/superzazu")
endif()
