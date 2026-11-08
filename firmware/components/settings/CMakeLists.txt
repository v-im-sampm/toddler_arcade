if(ESP_PLATFORM)
  idf_component_register(
    SRCS "nvs_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES audio
    PRIV_REQUIRES nvs_flash log
  )
endif()
