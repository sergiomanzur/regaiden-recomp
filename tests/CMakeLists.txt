set(RECOMPILED_DIR "${PROJECT_SOURCE_DIR}/src/recompiled")

add_executable(test_widescreen test_widescreen.c "${RECOMPILED_DIR}/widescreen_ppu.c")
add_executable(test_music_pack test_music_pack.c "${RECOMPILED_DIR}/music_pack.c")
add_executable(test_asset_dump test_asset_dump.c "${RECOMPILED_DIR}/asset_dump.c")

foreach(test_target IN ITEMS test_widescreen test_music_pack test_asset_dump)
    target_include_directories(${test_target} PRIVATE
        "${RECOMPILED_DIR}" "${RECOMPILED_DIR}/runtime/include"
        "${RECOMPILED_DIR}/runtime/vendor/stb")
    # Assertions are the test oracle, including in Release builds.
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES TIMEOUT 60)
endforeach()

target_compile_definitions(test_music_pack PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(test_music_pack PRIVATE SDL2::SDL2)
if(UNIX)
    target_link_libraries(test_music_pack PRIVATE m)
endif()
if(WIN32)
    add_custom_command(TARGET test_music_pack POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:test_music_pack>)
endif()
