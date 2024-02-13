#ifndef BUTTONS_H
#define BUTTONS_H

#include "esp_audio.h"
#include "esp_peripherals.h"

void buttons_start(esp_periph_set_handle_t set, esp_audio_handle_t player);

#endif