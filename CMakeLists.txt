cmake_minimum_required(VERSION 3.21)

project(timepilot3d VERSION 0.1.0 LANGUAGES C CXX)

# Time Pilot 3D for VectorPie. Built like the SDK's own programs and like the
# Tempest 2000 port (F:/Tempest 2000 Port):
#   PC, natively (MSYS2 MinGW-w64 with SDL2):  see BUILD.md
#   Pi, with clang against a copy of the cabinet's libraries: cmake/toolchain-pi-clang.cmake
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(TP3D_PI_BUILD)
    # cmake/toolchain-pi-clang.cmake: the cabinet's own SDL2, by its file.
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
        IMPORTED_SONAME "libSDL2-2.0.so.0"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include/SDL2"
        INTERFACE_COMPILE_DEFINITIONS "_REENTRANT")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/VectorPie/sdk vectorpie)

add_executable(timepilot3d
    src/main.c
    src/attract.c
    src/city.c
    src/controls.c
    src/vpbind.c
    src/draw.c
    src/boss.c
    src/enemies.c
    src/game.c
    src/hud.c
    src/logo.c
    src/models.c
    src/objects.c
    src/player.c
    src/service.c
    src/setup.c
    src/sound.c
    src/world.c)
target_include_directories(timepilot3d PRIVATE src)
vp_program(timepilot3d)

# The package's manifest, which also holds the settings: next to the
# executable, where vp_asset_path finds it.
add_custom_command(TARGET timepilot3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/package/game.ini
            $<TARGET_FILE_DIR:timepilot3d>/game.ini)

# The recordings that stand in for the made sounds (package/sounds), next to
# it too, so that a build on the PC sounds as the cabinet does.
add_custom_command(TARGET timepilot3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/package/sounds
            $<TARGET_FILE_DIR:timepilot3d>/sounds)

# and the music (package/music, tools/music_make.py)
add_custom_command(TARGET timepilot3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/package/music
            $<TARGET_FILE_DIR:timepilot3d>/music)
