idf_component_register(
  SRCS "main.cpp" "../../src/game/aquarium.cpp" "../../src/ui/aquarium_view.cpp" "../../src/ui/argb_image.cpp"
  INCLUDE_DIRS "../../src" "."
  REQUIRES board core audio settings lvgl__lvgl esp_psram heap spi_flash esp_partition esp_lcd esp_timer)
