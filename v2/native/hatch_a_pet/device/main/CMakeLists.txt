idf_component_register(
  SRCS "main.cpp" "../../model.cpp" "../../view.cpp"
       "../../../color_pop/src/ui/asset_pack.cpp"
  INCLUDE_DIRS "../.." "../../../color_pop/src" "."
  REQUIRES board core audio settings lvgl__lvgl esp_psram heap spi_flash esp_partition esp_lcd esp_timer)
