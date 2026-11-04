set(LA_AUDIO_SRCS volume.cpp synth.cpp sound_bank.cpp audio_feedback.cpp)

# Volume limiter, synth, sound bank and AudioFeedback are platform-free and
# host-tested; i2s_audio.cpp is the ESP-IDF I2S backend.
if(ESP_PLATFORM)
  idf_component_register(
    SRCS ${LA_AUDIO_SRCS} i2s_audio.cpp
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES board esp_driver_i2s esp_timer log
  )
else()
  list(TRANSFORM LA_AUDIO_SRCS PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
  add_library(little_arcade_audio STATIC ${LA_AUDIO_SRCS})
  target_include_directories(little_arcade_audio PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
  target_link_libraries(little_arcade_audio PUBLIC little_arcade_core)
  target_compile_features(little_arcade_audio PUBLIC cxx_std_17)
endif()
