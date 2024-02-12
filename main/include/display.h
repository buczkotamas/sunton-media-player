#ifndef DISPLAY_LVGL_H
#define DISPLAY_LVGL_H

#include "board.h"
#include "metadata.h"
#include "esp_lvgl_port.h"
#include "esp_audio.h"

#define LCD_BACKLIGHT_OFF_DUTY 0

typedef enum
{
    LCD_BACKLIGHT_OFF = 0,
    LCD_BACKLIGHT_DIMM = 1,
    LCD_BACKLIGHT_FULL = 2,
} lcd_backlight_state_t;

void lcd_backlight_set_duty(uint32_t target_duty, int max_fade_time_ms);
esp_err_t display_lvgl_init(esp_lcd_handle_t esp_lcd, esp_audio_handle_t audio_handle);
void display_lvgl_start(void);

#endif