set(COMPONENT_ADD_INCLUDEDIRS ./include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES espressif__esp_lcd_touch_gt911)
set(COMPONENT_PRIV_REQUIRES
    esp_peripherals 
    audio_sal
    audio_hal
    esp_dispatcher
    display_service
    audio_stream
    lvgl__lvgl
    espressif__esp_lvgl_port
    )

if (CONFIG_ESP32_S3_SUNTON_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_SUNTON_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./sunton-esp32-8048S070)
set(COMPONENT_SRCS
./sunton-esp32-8048S070/board.c
./sunton-esp32-8048S070/board_pins_config.c
)
endif()

if (CONFIG_ESP32_S3_DEVKIT_C_BOARD)
message(STATUS "Current board name is " CONFIG_ESP32_S3_DEVKIT_C_BOARD)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32-s3-devkitc-1-n16r8)
set(COMPONENT_SRCS
./esp32-s3-devkitc-1-n16r8/board.c
./esp32-s3-devkitc-1-n16r8/board_pins_config.c
)

endif()

register_component()
