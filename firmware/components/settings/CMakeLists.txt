# Parent settings: ParentGate is platform-free and host-tested; nvs_store.cpp
# persists the chosen volume on the device.
if(ESP_PLATFORM)
  idf_component_register(
    SRCS "parent_gate.cpp" "nvs_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core
    PRIV_REQUIRES nvs_flash log
  )
else()
  add_library(little_arcade_settings STATIC ${CMAKE_CURRENT_LIST_DIR}/parent_gate.cpp)
  target_include_directories(little_arcade_settings PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
  target_link_libraries(little_arcade_settings PUBLIC little_arcade_core)
  target_compile_features(little_arcade_settings PUBLIC cxx_std_17)
  if(COMMAND la_host_test)
    la_host_test(parent_gate_test ${CMAKE_CURRENT_LIST_DIR}/test/parent_gate_test.cpp)
    target_link_libraries(parent_gate_test PRIVATE little_arcade_settings)
  endif()
endif()
