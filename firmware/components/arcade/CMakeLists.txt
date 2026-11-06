# Arcade shell: games + SceneManager + LVGL views. Shared by the device
# firmware (main/) and the desktop simulator (sim/).
if(ESP_PLATFORM)
  idf_component_register(SRCS "arcade.cpp" "parent_view.cpp" INCLUDE_DIRS "include"
                         REQUIRES core ui settings animal_sounds beat_sharks bubble_aquarium color_pop
                                  feed_monster hatch_a_pet rainbow_piano robo_buddy rocket_rescue
                                  shape_garage silly_faces sound_jar lvgl__lvgl)
else()
  if(TARGET little_arcade_game_animal_sounds_view AND TARGET little_arcade_game_color_pop_view AND
     TARGET little_arcade_game_bubble_aquarium_view AND
     TARGET little_arcade_game_feed_monster_view AND
     TARGET little_arcade_game_hatch_a_pet_view AND
     TARGET little_arcade_game_rainbow_piano_view AND TARGET little_arcade_game_robo_buddy_view AND
     TARGET little_arcade_game_rocket_rescue_view AND TARGET little_arcade_game_silly_faces_view AND
     TARGET little_arcade_game_shape_garage_view AND TARGET little_arcade_game_sound_jar_view AND
     TARGET little_arcade_game_beat_sharks_view)
    add_library(little_arcade_arcade STATIC ${CMAKE_CURRENT_LIST_DIR}/arcade.cpp ${CMAKE_CURRENT_LIST_DIR}/parent_view.cpp)
    target_include_directories(little_arcade_arcade PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
    target_link_libraries(little_arcade_arcade PUBLIC little_arcade_game_animal_sounds_view
                          little_arcade_game_color_pop_view
                          little_arcade_game_feed_monster_view little_arcade_game_hatch_a_pet_view
                          little_arcade_game_rainbow_piano_view
                          little_arcade_game_bubble_aquarium_view little_arcade_game_rocket_rescue_view
                          little_arcade_game_silly_faces_view
                          little_arcade_game_robo_buddy_view little_arcade_game_shape_garage_view
                          little_arcade_game_sound_jar_view little_arcade_game_beat_sharks_view
                          little_arcade_ui little_arcade_settings)
  endif()
endif()
