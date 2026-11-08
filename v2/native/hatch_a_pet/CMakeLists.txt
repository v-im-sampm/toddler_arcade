set(HATCH_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
add_library(hatch_a_pet_game STATIC model.cpp)
target_include_directories(hatch_a_pet_game PUBLIC ${CMAKE_CURRENT_LIST_DIR})

find_program(HATCH_NODE node REQUIRED)
set(HATCH_PACK ${CMAKE_CURRENT_LIST_DIR}/assets/generated/hatch_a_pet.pack)
add_custom_command(OUTPUT ${HATCH_PACK}
  COMMAND ${HATCH_NODE} ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tools/export_assets.mjs ${HATCH_ROOT}/v2/reference/hatch_a_pet.html
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  COMMENT "Capturing original Hatch-a-Pet Canvas layers")
add_custom_target(hatch_a_pet_assets DEPENDS ${HATCH_PACK})

add_library(hatch_a_pet_view STATIC view.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../color_pop/src/ui/asset_pack.cpp)
target_include_directories(hatch_a_pet_view PUBLIC ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../color_pop/src)
target_link_libraries(hatch_a_pet_view PUBLIC hatch_a_pet_game lvgl)

add_executable(hatch_a_pet_native desktop.cpp)
target_sources(hatch_a_pet_native PRIVATE
  ${HATCH_ROOT}/firmware/components/audio/hatch_a_pet_score.cpp
  ${HATCH_ROOT}/firmware/components/audio/volume.cpp)
target_include_directories(hatch_a_pet_native PRIVATE
  ${HATCH_ROOT}/firmware/components/audio/include
  ${HATCH_ROOT}/firmware/components/core/include)
target_link_libraries(hatch_a_pet_native PRIVATE hatch_a_pet_view SDL2::SDL2-static)
add_dependencies(hatch_a_pet_native hatch_a_pet_assets)
add_custom_command(TARGET hatch_a_pet_native POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HATCH_PACK}
    $<TARGET_FILE_DIR:hatch_a_pet_native>/hatch_a_pet.pack)
if(MSVC)
  target_compile_definitions(hatch_a_pet_native PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(hatch_a_pet_game_test test/model_test.cpp)
target_link_libraries(hatch_a_pet_game_test PRIVATE hatch_a_pet_game)
add_test(NAME hatch_a_pet_game_test COMMAND hatch_a_pet_game_test)
add_test(NAME hatch_a_pet_offscreen_check COMMAND hatch_a_pet_native --check)
add_test(NAME hatch_a_pet_visual_compare
  COMMAND ${HATCH_NODE} ${CMAKE_CURRENT_LIST_DIR}/tools/visual_compare.mjs
    $<TARGET_FILE:hatch_a_pet_native>)
