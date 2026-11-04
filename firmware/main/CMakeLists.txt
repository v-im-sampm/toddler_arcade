idf_component_register(
  SRCS "main.cpp"
  REQUIRES core board esp_system esp_psram heap spi_flash esp_app_format
  PRIV_REQUIRES lvgl__lvgl
)
