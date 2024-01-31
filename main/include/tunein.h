#ifndef TUNEIN_H
#define TUNEIN_H

#include "esp_err.h"

typedef struct
{
    char *guide_id;
    char *image_url;
    char *title;
} now_playing_t;

typedef struct
{
    char *guide_id;
    char *stream_url;
    char *image_url;
    char *title;
    char *subtitle;
    char *description;
} radio_station_t;

esp_err_t tunein_favorites_get(radio_station_t **stations, int *count);
esp_err_t tunein_stream_url_get(radio_station_t *radio_station);

#endif