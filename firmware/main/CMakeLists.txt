idf_component_register(
  SRCS "main.cpp"
  REQUIRES core board arcade audio ui settings esp_system esp_psram heap spi_flash esp_app_format esp_timer esp_hw_support esp_lcd
  PRIV_REQUIRES lvgl__lvgl
)
