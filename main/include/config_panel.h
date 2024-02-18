#ifndef CONFIG_PANEL_H
#define CONFIG_PANEL_H

#include "gui.h"
#include "esp_err.h"
#include "esp_lvgl_port.h"

lv_obj_t *config_panel_create(lv_obj_t *parent);

#endif