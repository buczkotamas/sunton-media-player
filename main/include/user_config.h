#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "esp_err.h"

uint8_t config_get_i2s_output();
esp_err_t config_set_i2s_output(uint8_t value);

uint8_t config_get_audio_volume();
esp_err_t config_set_audio_volume(uint8_t value);

uint8_t config_get_backlight_full();
esp_err_t config_set_backlight_full(uint8_t value);

uint8_t config_get_backlight_dimm();
esp_err_t config_set_backlight_dimm(uint8_t value);

uint32_t config_get_backlight_timer();
esp_err_t config_set_backlight_timer(uint32_t value);

#endif