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

void tunein_browser_refresh(void);
lv_obj_t *tunein_browser_create(lv_obj_t *parent);

#endif