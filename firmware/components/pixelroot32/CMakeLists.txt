# Vendored PixelRoot32 engine + APU (MIT, see VENDORED.md). Built without the
# Arduino core and without SDL2: port/Arduino.h supplies the few Arduino
# symbols the compiled subset uses, and the arcade provides its own draw
# surface, touch feed and audio pull (components/games/color_pop/pr32).
set(LA_PR32_ENGINE_SRCS
  core/Engine.cpp core/Scene.cpp core/SceneManager.cpp
  graphics/Color.cpp graphics/DirtyGrid.cpp graphics/DisplayConfig.cpp graphics/Font5x7.cpp
  graphics/FontManager.cpp graphics/Renderer.cpp graphics/SpanTable.cpp graphics/TileAnimation.cpp
  graphics/TextLayout.cpp graphics/TransitionEffect.cpp
  input/InputManager.cpp input/TouchEventDispatcher.cpp input/TouchEventQueue.cpp
  input/TouchManager.cpp input/TouchStateMachine.cpp
  audio/AudioEngine.cpp audio/DefaultAudioScheduler.cpp audio/MusicPlayer.cpp
  platforms/PlatformCapabilities.cpp platforms/PlatformLog.cpp
  gameplay/GameplayEventBus.cpp)
list(TRANSFORM LA_PR32_ENGINE_SRCS PREPEND engine/src/)
set(LA_PR32_SRCS ${LA_PR32_ENGINE_SRCS} apu/src/ApuCore.cpp port/arduino_shim.cpp)
set(LA_PR32_INCLUDES port engine/include apu/include)

# Module selection shared by every translation unit that includes engine headers.
set(LA_PR32_DEFS
  PIXELROOT32_ENABLE_AUDIO=1
  PIXELROOT32_ENABLE_PHYSICS=0
  PIXELROOT32_ENABLE_UI_SYSTEM=0
  PIXELROOT32_ENABLE_PARTICLES=0
  PIXELROOT32_ENABLE_CAMERA_EFFECTS=0
  PIXELROOT32_ENABLE_SCENE_TRANSITIONS=0
  PIXELROOT32_ENABLE_STATIC_TILEMAP_FB_CACHE=0
  PIXELROOT32_ENABLE_TILE_ANIMATIONS=0
  PIXELROOT32_ENABLE_TOUCH=1
  PIXELROOT32_ENABLE_4BPP_SPRITES=1
  PIXELROOT32_NO_TFT_ESPI=1
  PIXELROOT32_NO_DAC_AUDIO=1
  PIXELROOT32_NO_I2S_AUDIO=1
  PHYSICAL_DISPLAY_WIDTH=320
  PHYSICAL_DISPLAY_HEIGHT=240)

if(ESP_PLATFORM)
  idf_component_register(SRCS ${LA_PR32_SRCS} INCLUDE_DIRS ${LA_PR32_INCLUDES})
  target_compile_definitions(${COMPONENT_LIB} PUBLIC ${LA_PR32_DEFS})
  # Third-party code: keep IDF's -Werror from failing on upstream warnings.
  target_compile_options(${COMPONENT_LIB} PRIVATE -w)
else()
  # Third-party code: do not inherit the arcade's /W4 /WX (or -Werror).
  set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
  list(TRANSFORM LA_PR32_SRCS PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
  list(TRANSFORM LA_PR32_INCLUDES PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
  add_library(little_arcade_pixelroot32 STATIC ${LA_PR32_SRCS})
  # SYSTEM: engine headers must not trip warnings-as-errors in arcade code.
  target_include_directories(little_arcade_pixelroot32 SYSTEM PUBLIC ${LA_PR32_INCLUDES})
  target_compile_definitions(little_arcade_pixelroot32 PUBLIC ${LA_PR32_DEFS})
  target_compile_features(little_arcade_pixelroot32 PUBLIC cxx_std_17)
  if(MSVC)
    target_compile_options(little_arcade_pixelroot32 PRIVATE /w /permissive- /bigobj /FI${CMAKE_CURRENT_LIST_DIR}/port/msvc_compat.h)
    target_compile_definitions(little_arcade_pixelroot32 PUBLIC _USE_MATH_DEFINES NOMINMAX)
  else()
    target_compile_options(little_arcade_pixelroot32 PRIVATE -w)
  endif()
endif()
