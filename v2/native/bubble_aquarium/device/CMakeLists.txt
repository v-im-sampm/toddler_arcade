cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/../../../../firmware/components/board
  ${CMAKE_CURRENT_LIST_DIR}/../../../../firmware/components/core
  ${CMAKE_CURRENT_LIST_DIR}/../../../../firmware/components/audio
  ${CMAKE_CURRENT_LIST_DIR}/../../../../firmware/components/settings)
set(SDKCONFIG_DEFAULTS ${CMAKE_CURRENT_LIST_DIR}/../../../../firmware/sdkconfig.defaults)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bubble_aquarium_sprite)
