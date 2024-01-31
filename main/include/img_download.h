#ifndef IMG_DOWNLOAD_H
#define IMG_DOWNLOAD_H

#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

esp_err_t download_image(char *url, lv_img_dsc_t *lv_img_dsc);

#endif