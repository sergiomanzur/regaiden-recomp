# Resident Evil Gaiden - Native C Recompilation
# Root CMake Configuration
cmake_minimum_required(VERSION 3.20)
project(ResidentEvilGaidenRecomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Locate SDL2
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL2-2.30.12")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL2-2.30.12")
endif()

if(WIN32)
    add_link_options(-Wl,/STACK:67108864)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Include the recompiled game sources
add_subdirectory(src/recompiled)

