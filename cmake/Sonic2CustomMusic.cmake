# Dynamic LGPL library; only the Genesis VGM reader is enabled. Keep the exact
# source available with releases so the player can replace/rebuild the DLL.
include(FetchContent)
set(SONIC2_GME_SOURCE_DIR "" CACHE PATH "Optional pinned Game_Music_Emu source checkout")
foreach(format AY GBS GYM HES KSS NSF NSFE SAP SPC)
    set(USE_GME_${format} OFF CACHE BOOL "" FORCE)
endforeach()
set(USE_GME_VGM ON CACHE BOOL "" FORCE)
set(GME_ZLIB OFF CACHE BOOL "" FORCE)
set(GME_YM2612_EMU "Nuked" CACHE STRING "" FORCE)
set(GME_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(GME_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(GME_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(GME_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(SONIC2_GME_SOURCE_DIR)
    set(_gme_input "${SONIC2_GME_SOURCE_DIR}")
else()
    FetchContent_Declare(sonic2_gme
        URL https://codeload.github.com/libgme/game-music-emu/zip/9e23d10f9fd2a6a2f33b10912dd8dc7153258995
        URL_HASH SHA256=4fe88326f27159a6d78b84666731b394b517388560d65931f5aff8b99fe19e6f)
    FetchContent_GetProperties(sonic2_gme)
    if(NOT sonic2_gme_POPULATED)
        FetchContent_Populate(sonic2_gme)
    endif()
    set(_gme_input "${sonic2_gme_SOURCE_DIR}")
endif()

# Build and distribute the exact patched source without modifying any shared
# source checkout. The patch sends DAC writes through Nuked's YM2612 channel 6.
find_package(Git REQUIRED)
set(_gme_source "${CMAKE_BINARY_DIR}/sonic2-gme-source")
set(_gme_patch "${CMAKE_CURRENT_LIST_DIR}/gme-ym2612-dac.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_gme_patch}")
file(COPY "${_gme_input}/gme" "${_gme_input}/cmake" DESTINATION "${_gme_source}")
foreach(_file CMakeLists.txt license.txt license.gpl2.txt readme.txt gme.txt)
    configure_file("${_gme_input}/${_file}" "${_gme_source}/${_file}" COPYONLY)
endforeach()
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_gme_patch}"
    WORKING_DIRECTORY "${_gme_source}" RESULT_VARIABLE _gme_patched OUTPUT_QUIET ERROR_QUIET)
if(NOT _gme_patched EQUAL 0)
    foreach(_file Vgm_Emu.cpp Vgm_Emu_Impl.cpp Vgm_Emu_Impl.h)
        configure_file("${_gme_input}/gme/${_file}" "${_gme_source}/gme/${_file}" COPYONLY)
    endforeach()
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${_gme_patch}"
        WORKING_DIRECTORY "${_gme_source}" RESULT_VARIABLE _gme_patch_status)
    if(NOT _gme_patch_status EQUAL 0)
        message(FATAL_ERROR "Unable to apply the YM2612 DAC patch to the pinned Game_Music_Emu source")
    endif()
endif()
add_subdirectory("${_gme_source}" "${CMAKE_BINARY_DIR}/gme" EXCLUDE_FROM_ALL)

function(sonic2_link_custom_music target)
    target_link_libraries(${target} PRIVATE gme::gme)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:gme_shared>" "$<TARGET_FILE_DIR:${target}>")
    if(UNIX)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_SONAME_FILE:gme_shared>" "$<TARGET_FILE_DIR:${target}>")
        if(APPLE)
            set_target_properties(${target} PROPERTIES BUILD_RPATH "@loader_path" INSTALL_RPATH "@loader_path")
        else()
            set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN ON)
        endif()
    endif()
    if(target STREQUAL "SonicTheHedgehog2Recomp")
        get_target_property(_gme_dir gme_shared SOURCE_DIR)
        get_filename_component(_gme_root "${_gme_dir}" DIRECTORY)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/docs/libgme-source"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${_gme_root}/gme" "$<TARGET_FILE_DIR:${target}>/docs/libgme-source/gme"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${_gme_root}/cmake" "$<TARGET_FILE_DIR:${target}>/docs/libgme-source/cmake"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_gme_root}/CMakeLists.txt" "${_gme_root}/license.txt" "${_gme_root}/license.gpl2.txt" "${_gme_root}/readme.txt" "${_gme_root}/gme.txt" "$<TARGET_FILE_DIR:${target}>/docs/libgme-source"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/docs/CUSTOM_MUSIC.md" "$<TARGET_FILE_DIR:${target}>/docs")
    endif()
endfunction()
