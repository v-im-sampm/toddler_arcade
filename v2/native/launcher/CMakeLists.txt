set(LAUNCHER_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(LAUNCHER_FW ${LAUNCHER_ROOT}/firmware/components)

# Shared, platform-free little_arcade core + audio (AudioFeedback, synth, limiter) for host builds.
if(NOT TARGET little_arcade_core)
  add_subdirectory(${LAUNCHER_FW}/core ${CMAKE_BINARY_DIR}/little_arcade_core)
endif()
if(NOT TARGET little_arcade_audio)
  add_subdirectory(${LAUNCHER_FW}/audio ${CMAKE_BINARY_DIR}/little_arcade_audio)
endif()

add_library(launcher_shell STATIC src/launcher/shell.cpp)
target_include_directories(launcher_shell PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src ${LAUNCHER_FW}/audio/include)
target_compile_features(launcher_shell PUBLIC cxx_std_17)

add_executable(launcher_test test/launcher_test.cpp)
target_link_libraries(launcher_test PRIVATE launcher_shell)
add_test(NAME launcher_test COMMAND launcher_test)

add_executable(launcher_native
  src/platform/desktop/main.cpp
  src/launcher/arcade.cpp
  src/launcher/menu_view.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../bubble_aquarium/src/ui/aquarium_view.cpp)
target_link_libraries(launcher_native PRIVATE launcher_shell color_pop_view color_pop_score aquarium_game aquarium_image
                      little_arcade_audio SDL2::SDL2-static)
add_dependencies(launcher_native color_pop_assets aquarium_assets)
set(LAUNCHER_CP_PACK ${CMAKE_CURRENT_LIST_DIR}/../color_pop/assets/generated/color_pop.pack)
set(LAUNCHER_AQ_ASSETS ${CMAKE_CURRENT_LIST_DIR}/../bubble_aquarium/assets/generated)
add_custom_command(TARGET launcher_native POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LAUNCHER_CP_PACK} $<TARGET_FILE_DIR:launcher_native>/color_pop.pack
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${LAUNCHER_AQ_ASSETS} $<TARGET_FILE_DIR:launcher_native>/assets)
if(MSVC)
  target_compile_definitions(launcher_native PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_test(NAME launcher_offscreen_check
  COMMAND launcher_native --check --pack ${LAUNCHER_CP_PACK} --aquarium-assets ${LAUNCHER_AQ_ASSETS})
