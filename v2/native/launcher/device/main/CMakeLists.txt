get_filename_component(LA_NATIVE "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
idf_component_register(
  SRCS "main.cpp"
       "${LA_NATIVE}/launcher/src/launcher/shell.cpp"
       "${LA_NATIVE}/launcher/src/launcher/menu_view.cpp"
       "${LA_NATIVE}/launcher/src/launcher/arcade.cpp"
       "${LA_NATIVE}/color_pop/src/game/color_pop.cpp"
       "${LA_NATIVE}/color_pop/src/audio/color_pop_score.cpp"
       "${LA_NATIVE}/color_pop/src/ui/asset_pack.cpp"
       "${LA_NATIVE}/color_pop/src/ui/color_pop_view.cpp"
       "${LA_NATIVE}/bubble_aquarium/src/game/aquarium.cpp"
       "${LA_NATIVE}/bubble_aquarium/src/ui/aquarium_view.cpp"
       "${LA_NATIVE}/bubble_aquarium/src/ui/argb_image.cpp"
  INCLUDE_DIRS "${LA_NATIVE}/launcher/src" "${LA_NATIVE}/color_pop/src" "${LA_NATIVE}/bubble_aquarium/src" "."
               "${LA_NATIVE}/bubble_aquarium/device/main" "${LA_NATIVE}/color_pop/device/main"
  REQUIRES board core audio settings lvgl__lvgl esp_psram heap spi_flash esp_partition esp_lcd esp_timer)
