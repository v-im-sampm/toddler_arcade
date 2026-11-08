set(COLOR_POP_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(COLOR_POP_FW ${COLOR_POP_ROOT}/firmware/components)

add_library(color_pop_game STATIC src/game/color_pop.cpp)
target_include_directories(color_pop_game PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src ${COLOR_POP_FW}/core/include)
target_compile_features(color_pop_game PUBLIC cxx_std_17)

# Score + the platform-free shared volume limiter (same sources the firmware audio component uses).
add_library(color_pop_score STATIC src/audio/color_pop_score.cpp ${COLOR_POP_FW}/audio/volume.cpp)
target_include_directories(color_pop_score PUBLIC ${COLOR_POP_FW}/audio/include)
target_link_libraries(color_pop_score PUBLIC color_pop_game)

find_program(COLOR_POP_NODE node REQUIRED)
set(COLOR_POP_ASSETS ${CMAKE_CURRENT_LIST_DIR}/assets/generated)
add_custom_command(OUTPUT ${COLOR_POP_ASSETS}/color_pop.pack
  COMMAND ${COLOR_POP_NODE} ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
          ${COLOR_POP_ROOT}/v2/reference/quality_bar.html
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  COMMENT "Exporting original Color Pop Canvas artwork into the LVGL sprite pack")
add_custom_target(color_pop_assets DEPENDS ${COLOR_POP_ASSETS}/color_pop.pack)

add_library(color_pop_view STATIC src/ui/asset_pack.cpp src/ui/color_pop_view.cpp)
target_include_directories(color_pop_view PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(color_pop_view PUBLIC color_pop_game lvgl)

add_executable(color_pop_native src/platform/desktop/main.cpp)
target_link_libraries(color_pop_native PRIVATE color_pop_view color_pop_score SDL2::SDL2-static)
add_dependencies(color_pop_native color_pop_assets)
add_custom_command(TARGET color_pop_native POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COLOR_POP_ASSETS}/color_pop.pack
          $<TARGET_FILE_DIR:color_pop_native>/color_pop.pack)
if(MSVC)
  target_compile_definitions(color_pop_native PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(color_pop_view PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(color_pop_game_test test/game_test.cpp)
target_link_libraries(color_pop_game_test PRIVATE color_pop_game)
add_test(NAME color_pop_game_test COMMAND color_pop_game_test)

add_executable(color_pop_score_test test/score_test.cpp)
target_link_libraries(color_pop_score_test PRIVATE color_pop_score)
add_test(NAME color_pop_score_test COMMAND color_pop_score_test)

add_test(NAME color_pop_offscreen_check
  COMMAND color_pop_native --check --seed 1234 --pack ${COLOR_POP_ASSETS}/color_pop.pack)
