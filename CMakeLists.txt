cmake_minimum_required(VERSION 3.16)
project(MarioIsMissingRecomp C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# SDL2
find_package(SDL2 REQUIRED)

# --- snesrecomp: real SNES hardware backend (LakeSnes + adapter) ---
set(SNESRECOMP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/snesrecomp)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- mim_game (static lib: recompiled game code) ---
add_library(mim_game STATIC
    src/recomp/mim_boot.c
)
target_link_libraries(mim_game PUBLIC snesrecomp)

# --- mim_launcher (executable) ---
add_executable(mim_launcher src/main/main.c)
target_link_libraries(mim_launcher PRIVATE
    mim_game
    snesrecomp
    SDL2::SDL2main
)
