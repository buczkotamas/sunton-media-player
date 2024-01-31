#ifndef TUNEIN_H
#define TUNEIN_H

#include "esp_err.h"
#include "esp_lvgl_port.h"

typedef struct
{
    char *guide_id;
    char *stream_url;
    char *image_url;
    char *title;
    char *subtitle;
    char *description;
} radio_station_t;

esp_err_t tunein_stream_url_get(radio_station_t *radio_station);
lv_obj_t *tunein_browser_create(lv_obj_t *parent, lv_event_cb_t event_cb);

#endif