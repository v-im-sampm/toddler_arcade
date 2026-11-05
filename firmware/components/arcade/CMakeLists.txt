# Arcade shell: games + SceneManager + LVGL views. Shared by the device
# firmware (main/) and the desktop simulator (sim/).
if(ESP_PLATFORM)
  idf_component_register(SRCS "arcade.cpp" "parent_view.cpp" INCLUDE_DIRS "include"
                         REQUIRES core ui settings color_pop hatch_a_pet lvgl__lvgl)
else()
  if(TARGET little_arcade_game_color_pop_view AND TARGET little_arcade_game_hatch_a_pet_view)
    add_library(little_arcade_arcade STATIC ${CMAKE_CURRENT_LIST_DIR}/arcade.cpp ${CMAKE_CURRENT_LIST_DIR}/parent_view.cpp)
    target_include_directories(little_arcade_arcade PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    target_link_libraries(little_arcade_arcade PUBLIC little_arcade_game_color_pop_view
                          little_arcade_game_hatch_a_pet_view little_arcade_ui little_arcade_settings)
  endif()
endif()
