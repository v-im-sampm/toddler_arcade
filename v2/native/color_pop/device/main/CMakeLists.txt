idf_component_register(
  SRCS "main.cpp" "../../src/game/color_pop.cpp" "../../src/audio/color_pop_score.cpp"
       "../../src/ui/asset_pack.cpp" "../../src/ui/color_pop_view.cpp"
  INCLUDE_DIRS "../../src" "."
  REQUIRES board core audio settings lvgl__lvgl esp_psram heap spi_flash esp_partition esp_lcd esp_timer)

# Bench firmware (build.ps1 -Bench): auto-taps targets to measure worst-case frames without a child.
if(COLOR_POP_BENCH)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE COLOR_POP_BENCH=1)
endif()
