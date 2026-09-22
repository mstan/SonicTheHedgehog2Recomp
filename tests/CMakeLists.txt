# Sonic 2 implementation tests belong to the game, never the shared engine.
set(S2_GAME "${CMAKE_CURRENT_LIST_DIR}/../game")
set(S2_TOOLS "${CMAKE_CURRENT_LIST_DIR}/../tools")

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
s2_test(sonic2_party "${S2_GAME}/sonic2_party.c")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/party-fixtures")
set_tests_properties(sonic2_party PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/party-fixtures")
s2_test(sonic2_donor "${S2_GAME}/sonic2_donor_assets.c")
s2_test(sonic2_video)
s2_test(sonic2_character "${S2_GAME}/sonic2_character.c")

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
        "${S2_GAME}/sonic2_resources.c" "${S2_GAME}/sonic2_save_assets.c"
        "${S2_GAME}/sonic2_donor_assets.c" "${S2_GAME}/sonic2_mods.c" "${S2_SHA}")
    target_compile_definitions(sonic2_resources_test PRIVATE RECOMP_LAUNCHER=1)
    target_include_directories(sonic2_resources_test PRIVATE
        "${CMAKE_SOURCE_DIR}/recomp-ui/src" "${CMAKE_SOURCE_DIR}/recomp-ui/src/common")
endif()
