add_library(aquarium_game STATIC src/game/aquarium.cpp)
target_include_directories(aquarium_game PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_compile_features(aquarium_game PUBLIC cxx_std_17)

find_program(AQUARIUM_NODE node REQUIRED)
set(AQUARIUM_ASSETS ${CMAKE_CURRENT_LIST_DIR}/assets/generated)
add_custom_command(OUTPUT ${AQUARIUM_ASSETS}/manifest.json
  COMMAND ${AQUARIUM_NODE} ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
          ${CMAKE_CURRENT_LIST_DIR}/../../reference/bubble_aquarium.html
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  COMMENT "Exporting original Canvas artwork into PNG and LVGL BGRA sprites")
add_custom_target(aquarium_assets DEPENDS ${AQUARIUM_ASSETS}/manifest.json)

add_executable(bubble_aquarium_native src/platform/desktop/main.cpp src/ui/aquarium_view.cpp)
target_include_directories(bubble_aquarium_native PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(bubble_aquarium_native PRIVATE aquarium_game little_arcade_ui SDL2::SDL2-static)
add_dependencies(bubble_aquarium_native aquarium_assets)
add_custom_command(TARGET bubble_aquarium_native POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${AQUARIUM_ASSETS} $<TARGET_FILE_DIR:bubble_aquarium_native>/assets)
if(MSVC)
  target_compile_definitions(bubble_aquarium_native PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(aquarium_game_test test/game_test.cpp)
target_link_libraries(aquarium_game_test PRIVATE aquarium_game)
enable_testing()
add_test(NAME aquarium_game_test COMMAND aquarium_game_test)
