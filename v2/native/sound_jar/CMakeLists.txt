set(SOUND_JAR_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(SOUND_JAR_FW ${SOUND_JAR_ROOT}/firmware/components)
set(SOUND_JAR_ASSETS ${CMAKE_CURRENT_LIST_DIR}/assets/generated)

# The exporter runs the reference's own Canvas drawing code (and synthesises the built-in voice
# takes) at build time; nothing from the browser is needed at run time.
find_program(SOUND_JAR_NODE node REQUIRED)
add_custom_command(OUTPUT ${SOUND_JAR_ASSETS}/sound_jar.pack ${SOUND_JAR_ASSETS}/takes_data.cpp
  COMMAND ${SOUND_JAR_NODE} ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
          ${SOUND_JAR_ROOT}/v2/reference/sound_jar.html
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  COMMENT "Exporting original Sound Jar Canvas artwork and takes")
add_custom_target(sound_jar_assets DEPENDS ${SOUND_JAR_ASSETS}/sound_jar.pack ${SOUND_JAR_ASSETS}/takes_data.cpp)

add_library(sound_jar_game STATIC src/game/sound_jar.cpp ${SOUND_JAR_ASSETS}/takes_data.cpp)
target_include_directories(sound_jar_game PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src ${SOUND_JAR_FW}/core/include)
target_compile_features(sound_jar_game PUBLIC cxx_std_17)
add_dependencies(sound_jar_game sound_jar_assets)

# Score + the platform-free shared volume limiter (same sources the firmware audio component uses).
add_library(sound_jar_score STATIC src/audio/sound_jar_score.cpp ${SOUND_JAR_FW}/audio/volume.cpp)
target_include_directories(sound_jar_score PUBLIC ${SOUND_JAR_FW}/audio/include)
target_link_libraries(sound_jar_score PUBLIC sound_jar_game)

add_library(sound_jar_view STATIC src/ui/asset_pack.cpp src/ui/sound_jar_view.cpp)
target_include_directories(sound_jar_view PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(sound_jar_view PUBLIC sound_jar_game lvgl)

add_executable(sound_jar_native src/platform/desktop/main.cpp)
target_link_libraries(sound_jar_native PRIVATE sound_jar_view sound_jar_score SDL2::SDL2-static)
add_custom_command(TARGET sound_jar_native POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOUND_JAR_ASSETS}/sound_jar.pack
          $<TARGET_FILE_DIR:sound_jar_native>/sound_jar.pack)
if(MSVC)
  target_compile_definitions(sound_jar_native PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(sound_jar_view PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(sound_jar_game_test test/game_test.cpp)
target_link_libraries(sound_jar_game_test PRIVATE sound_jar_game)
add_test(NAME sound_jar_game_test COMMAND sound_jar_game_test)

add_executable(sound_jar_score_test test/score_test.cpp)
target_link_libraries(sound_jar_score_test PRIVATE sound_jar_score)
add_test(NAME sound_jar_score_test COMMAND sound_jar_score_test)

add_test(NAME sound_jar_offscreen_check
  COMMAND sound_jar_native --check --seed 1234 --pack ${SOUND_JAR_ASSETS}/sound_jar.pack)
