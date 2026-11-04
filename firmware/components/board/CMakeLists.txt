if(ESP_PLATFORM)
  idf_component_register(
    SRCS "board_io.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_spi log
  )
else()
  add_library(little_arcade_board_pins INTERFACE)
  target_include_directories(little_arcade_board_pins INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
  target_compile_features(little_arcade_board_pins INTERFACE cxx_std_17)
endif()
