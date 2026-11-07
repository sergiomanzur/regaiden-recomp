cmake_minimum_required(VERSION 3.16)
project(Resident_Evil_Gaiden_Android C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Build SDL2 for Android
add_subdirectory(SDL2)

# Path to project root and recompiled sources
set(REGAIDEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(RECOMPILED_DIR "${REGAIDEN_ROOT}/src/recompiled")
set(GBRT_DIR "${RECOMPILED_DIR}/runtime")

include_directories(
    ${GBRT_DIR}/include
    ${GBRT_DIR}/vendor/imgui
    ${GBRT_DIR}/vendor/stb
    ${RECOMPILED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2/include
)

# Collect all recompiled chunks and funcs
file(GLOB RECOMPILED_CHUNKS "${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA__dispatch_chunk_*.c")
file(GLOB RECOMPILED_FUNCS "${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA__funcs_*.c")

# The largest generated unit takes over ten minutes in the NDK's -O3 optimizer
# alone. Use the desktop generated-code optimization level for this unit;
# keep the existing Android optimization for the rest of the game and runtime.
set_source_files_properties("${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA__funcs_95.c"
    PROPERTIES COMPILE_OPTIONS "-O1")

add_library(main SHARED
    ${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA__main.c
    ${RECOMPILED_DIR}/Resident_Evil_Gaiden__USA_.c
    ${RECOMPILED_CHUNKS}
    ${RECOMPILED_FUNCS}
    ${RECOMPILED_DIR}/widescreen_ppu.c
    ${RECOMPILED_DIR}/state_dump.c
    ${RECOMPILED_DIR}/asset_dump.c
    ${RECOMPILED_DIR}/music_pack.c
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

# Keep both JNI libraries loadable on Android devices with 16 KiB pages.
# Gradle selects the static STL because NDK r26's shared STL is 4 KiB aligned.
foreach(native_target IN ITEMS SDL2 main)
    target_link_options(${native_target} PRIVATE
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,common-page-size=16384")
endforeach()
