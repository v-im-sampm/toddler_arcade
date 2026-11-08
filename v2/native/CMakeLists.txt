cmake_minimum_required(VERSION 3.16)
project(little_arcade_v2_native C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
  URL_HASH SHA256=5F5993C530F084535C65A6879E9B26AD441169B3E25D789D83287040A9CA5165
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(lvgl
  URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz
  URL_HASH SHA256=34A955CDF3A2D005507B704E87357AF669A114523B6D3F77B5344FDC68717BC6
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

set(LV_BUILD_CONF_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)
target_link_libraries(lvgl PUBLIC SDL2::SDL2-static)
target_compile_definitions(lvgl PUBLIC SDL_MAIN_HANDLED)

enable_testing()
add_subdirectory(bubble_aquarium)
add_subdirectory(color_pop)
add_subdirectory(launcher)
add_subdirectory(sound_jar)
add_subdirectory(hatch_a_pet)
