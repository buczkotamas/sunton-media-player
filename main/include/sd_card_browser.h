#ifndef LV_FILE_BROWSER_H
#define LV_FILE_BROWSER_H

#include "esp_lvgl_port.h"

esp_err_t sd_card_browser_prev(void);
esp_err_t sd_card_browser_next(void);
lv_obj_t *sd_card_browser_create(lv_obj_t *parent, bool scan);

#endif