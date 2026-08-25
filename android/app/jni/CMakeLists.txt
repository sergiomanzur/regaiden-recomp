cmake_minimum_required(VERSION 3.16)
project(Resident_Evil_Gaiden_Android C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Path to project root and recompiled sources
set(REGAIDEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
set(RECOMPILED_DIR "${REGAIDEN_ROOT}/src/recompiled")
set(GBRT_DIR "${RECOMPILED_DIR}/runtime")

# Find / include SDL2
if(DEFINED ENV{SDL2_SOURCE_DIR})
    set(SDL2_PATH "$ENV{SDL2_SOURCE_DIR}")
elseif(EXISTS "${REGAIDEN_ROOT}/deps/SDL2-2.30.12")
    set(SDL2_PATH "${REGAIDEN_ROOT}/deps/SDL2-2.30.12")
endif()

include_directories(
    ${GBRT_DIR}/include
    ${GBRT_DIR}/vendor/imgui
    ${GBRT_DIR}/vendor/stb
    ${RECOMPILED_DIR}
    ${SDL2_PATH}/include
)

# Collect all recompiled functions and runtime files
file(GLOB RECOMPILED_FUNCS "${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA__funcs_*.c")

add_library(main SHARED
    ${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA__main.c
    ${RECOMPILED_FUNCS}
    ${RECOMPILED_DIR}/widescreen_ppu.c
    ${RECOMPILED_DIR}/lighting.c
    ${RECOMPILED_DIR}/postprocess.c
    ${RECOMPILED_DIR}/hd_pack.c
    ${RECOMPILED_DIR}/cheats.c
    ${RECOMPILED_DIR}/config_ini.c
    ${RECOMPILED_DIR}/rom_loader.c
    ${RECOMPILED_DIR}/touch_overlay.c
    ${GBRT_DIR}/src/gbrt.c
    ${GBRT_DIR}/src/gbrt_data_mod.c
    ${GBRT_DIR}/src/gbrt_hash.c
    ${GBRT_DIR}/src/gbrt_host_configuration.c
    ${GBRT_DIR}/src/gbrt_port.c
    ${GBRT_DIR}/src/gbrt_presentation.c
    ${GBRT_DIR}/src/gbrt_semantic.c
    ${GBRT_DIR}/src/differential.c
    ${GBRT_DIR}/src/ppu.c
    ${GBRT_DIR}/src/audio.c
    ${GBRT_DIR}/src/audio_stats.c
    ${GBRT_DIR}/src/interpreter.c
    ${GBRT_DIR}/src/platform_sdl.cpp
    ${GBRT_DIR}/vendor/imgui/imgui.cpp
    ${GBRT_DIR}/vendor/imgui/imgui_draw.cpp
    ${GBRT_DIR}/vendor/imgui/imgui_tables.cpp
    ${GBRT_DIR}/vendor/imgui/imgui_widgets.cpp
    ${GBRT_DIR}/vendor/imgui/backends/imgui_impl_sdl2.cpp
    ${GBRT_DIR}/vendor/imgui/backends/imgui_impl_sdlrenderer2.cpp
)

target_compile_definitions(main PRIVATE
    GB_HAS_SDL2=1
    __ANDROID__=1
)

target_link_libraries(main PRIVATE
    SDL2
    log
    android
    GLESv2
    GLESv3
    OpenSLES
)
