#ifndef GUI_H
#define GUI_H

#define UI_PADDING_ALL 8

#include "esp_lvgl_port.h"

/* Media */
#define UI_MEDIA_FOOTER_HIGHT 120
#define UI_MEDIA_ALBUM_ART_WIDTH 480
#define UI_MEDIA_ALBUM_ART_HIGHT 240

static const lv_font_t *UI_FONT_XS = &lv_font_montserrat_16;
static const lv_font_t *UI_FONT_S = &lv_font_montserrat_18;
static const lv_font_t *UI_FONT_M = &lv_font_montserrat_20;
static const lv_font_t *UI_FONT_L = &lv_font_montserrat_22;
static const lv_font_t *UI_FONT_XL = &lv_font_montserrat_24;

lv_style_t *gui_style_btnmatrix_main(void);
lv_style_t *gui_style_btnmatrix_items(void);

lv_style_t *gui_style_dropdown(void);

#endif