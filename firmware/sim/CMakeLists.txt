# Little Arcade desktop simulator: real game logic + real LVGL views on SDL2.
# Desktop only. Results are NOT hardware validation (AGENTS.md rule 8).
cmake_minimum_required(VERSION 3.16)
project(little_arcade_sim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Pinned dependencies. LVGL matches firmware/main/idf_component.yml (9.5.0,
# commit 85aa60d). To reuse an existing checkout (for example the IDF managed
# component) pass -DFETCHCONTENT_SOURCE_DIR_LVGL=<path>.
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

# Our code: strict warnings, applied per target so LVGL/SDL keep their own flags.
set(LA_COMPONENTS ${CMAKE_CURRENT_LIST_DIR}/../components)
add_subdirectory(${LA_COMPONENTS}/core core)
add_subdirectory(${LA_COMPONENTS}/ui ui)
add_subdirectory(${LA_COMPONENTS}/games/color_pop games/color_pop)
add_subdirectory(${LA_COMPONENTS}/games/hatch_a_pet games/hatch_a_pet)
add_subdirectory(${LA_COMPONENTS}/arcade arcade)

add_executable(little_arcade_sim main.cpp)
target_link_libraries(little_arcade_sim PRIVATE little_arcade_arcade)
if(MSVC)
  target_compile_definitions(little_arcade_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

foreach(t little_arcade_core little_arcade_ui little_arcade_game_color_pop
          little_arcade_game_color_pop_view little_arcade_game_hatch_a_pet
          little_arcade_game_hatch_a_pet_view little_arcade_arcade little_arcade_sim)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endforeach()
