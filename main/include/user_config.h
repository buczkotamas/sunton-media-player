#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "esp_err.h"

uint8_t config_get_i2s_output();
esp_err_t config_set_i2s_output(uint8_t value);

uint8_t config_get_audio_volume();
esp_err_t config_set_audio_volume(uint8_t value);

#endif