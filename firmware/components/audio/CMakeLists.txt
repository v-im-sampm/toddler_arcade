set(LA_AUDIO_SRCS volume.cpp synth.cpp sound_bank.cpp audio_feedback.cpp)

# Recorded animal calls are generated from committed WAVs at configure time.
# Early expansion only collects requirements, so skip file generation then.
set(LA_ANIMAL_CLIPS_CPP ${CMAKE_CURRENT_BINARY_DIR}/animal_clips.cpp)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
  include(${CMAKE_CURRENT_LIST_DIR}/animal_clips.cmake)
  la_generate_animal_clips(${CMAKE_CURRENT_LIST_DIR}/../../../assets/audio/animal_sounds ${LA_ANIMAL_CLIPS_CPP})
endif()

# Volume limiter, synth, sound bank and AudioFeedback are platform-free and
# host-tested; i2s_audio.cpp is the ESP-IDF I2S backend.
if(ESP_PLATFORM)
  idf_component_register(
    SRCS ${LA_AUDIO_SRCS} ${LA_ANIMAL_CLIPS_CPP} i2s_audio.cpp
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES board esp_driver_i2s esp_timer log
  )
else()
  list(TRANSFORM LA_AUDIO_SRCS PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
  add_library(little_arcade_audio STATIC ${LA_AUDIO_SRCS} ${LA_ANIMAL_CLIPS_CPP})
  target_include_directories(little_arcade_audio PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
  target_link_libraries(little_arcade_audio PUBLIC little_arcade_core)
  target_compile_features(little_arcade_audio PUBLIC cxx_std_17)
endif()
