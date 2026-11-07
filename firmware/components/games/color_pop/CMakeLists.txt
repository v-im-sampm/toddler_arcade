set(LA_COLOR_POP_PR32_SRCS pr32/color_pop_stage.cpp pr32/color_pop_scene.cpp pr32/color_pop_audio.cpp
                           pr32/frame_surface.cpp pr32/color_pop_sprites.cpp)
if(ESP_PLATFORM)
  # Game logic + its PixelRoot32 runtime (components/pixelroot32); view/include
  # holds the launcher tile art.
  idf_component_register(SRCS "color_pop.cpp" ${LA_COLOR_POP_PR32_SRCS}
                         INCLUDE_DIRS "include" "view/include" "pr32/include"
                         PRIV_INCLUDE_DIRS "pr32"
                         REQUIRES core ui audio
                         PRIV_REQUIRES pixelroot32 heap esp_timer)
else()
  add_library(little_arcade_game_color_pop STATIC ${CMAKE_CURRENT_LIST_DIR}/color_pop.cpp)
  target_include_directories(little_arcade_game_color_pop PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
  target_link_libraries(little_arcade_game_color_pop PUBLIC little_arcade_core)
  # PixelRoot32 runtime: the engine owns Color Pop's frame, touch and sound.
  if(TARGET little_arcade_pixelroot32 AND TARGET little_arcade_audio)
    list(TRANSFORM LA_COLOR_POP_PR32_SRCS PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
    add_library(little_arcade_game_color_pop_pr32 STATIC ${LA_COLOR_POP_PR32_SRCS})
    target_include_directories(little_arcade_game_color_pop_pr32 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/pr32/include
                               PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pr32)
    target_link_libraries(little_arcade_game_color_pop_pr32 PUBLIC little_arcade_game_color_pop little_arcade_audio
                          PRIVATE little_arcade_pixelroot32)
  endif()
  # Arcade-facing bundle: launcher tile art + the runtime (desktop simulator).
  if(TARGET lvgl AND TARGET little_arcade_ui AND TARGET little_arcade_game_color_pop_pr32)
    add_library(little_arcade_game_color_pop_view INTERFACE)
    target_include_directories(little_arcade_game_color_pop_view INTERFACE ${CMAKE_CURRENT_LIST_DIR}/view/include)
    target_link_libraries(little_arcade_game_color_pop_view INTERFACE little_arcade_game_color_pop_pr32 little_arcade_ui)
  endif()
  if(COMMAND la_host_test)
    la_host_test(color_pop_test ${CMAKE_CURRENT_LIST_DIR}/test/color_pop_test.cpp)
    target_link_libraries(color_pop_test PRIVATE little_arcade_game_color_pop)
    if(TARGET little_arcade_game_color_pop_pr32)
      la_host_test(color_pop_pr32_test ${CMAKE_CURRENT_LIST_DIR}/test/color_pop_pr32_test.cpp)
      target_include_directories(color_pop_pr32_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/pr32)
      target_link_libraries(color_pop_pr32_test PRIVATE little_arcade_game_color_pop_pr32 little_arcade_pixelroot32)
    endif()
    if(TARGET little_arcade_ui_raster)
      la_host_test(color_pop_patterns_test ${CMAKE_CURRENT_LIST_DIR}/test/color_pop_patterns_test.cpp)
      target_include_directories(color_pop_patterns_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/view/include)
      target_link_libraries(color_pop_patterns_test PRIVATE little_arcade_game_color_pop little_arcade_ui_raster)
    endif()
  endif()
endif()