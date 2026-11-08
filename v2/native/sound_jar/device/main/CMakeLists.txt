idf_component_register(
  SRCS "main.cpp" "../../src/game/sound_jar.cpp" "../../src/game/live_take.cpp" "../../assets/generated/takes_data.cpp"
       "../../src/audio/sound_jar_score.cpp" "../../src/ui/asset_pack.cpp" "../../src/ui/sound_jar_view.cpp"
  INCLUDE_DIRS "../../src" "."
  REQUIRES board core audio settings lvgl__lvgl esp_psram heap spi_flash esp_partition esp_lcd esp_timer)

# Bench firmware (build.ps1 -Bench): taps the reference autoTarget() so worst-case frames and audio load can be
# measured without a child.
if(SOUND_JAR_BENCH)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE SOUND_JAR_BENCH=1)
endif()
