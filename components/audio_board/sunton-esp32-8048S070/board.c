/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>

#include "periph_sdcard.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "esp_lcd_touch_gt911.h"

static const char *TAG = "AUDIO_BOARD";

static audio_board_handle_t board_handle = 0;
static esp_lcd_touch_handle_t touch_handle;
static esp_lcd_panel_io_handle_t io_handle = NULL;

#define LCD_PIXEL_CLOCK_HZ (12 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL

const gpio_num_t PIN_NUM_BK_LIGHT = GPIO_NUM_2;
const gpio_num_t PIN_NUM_HSYNC = GPIO_NUM_39;
const gpio_num_t PIN_NUM_VSYNC = GPIO_NUM_40;
const gpio_num_t PIN_NUM_DE = GPIO_NUM_41;
const gpio_num_t PIN_NUM_PCLK = GPIO_NUM_42;
const gpio_num_t PIN_NUM_DATA0 = GPIO_NUM_15;  // B0
const gpio_num_t PIN_NUM_DATA1 = GPIO_NUM_7;   // B1
const gpio_num_t PIN_NUM_DATA2 = GPIO_NUM_6;   // B2
const gpio_num_t PIN_NUM_DATA3 = GPIO_NUM_5;   // B3
const gpio_num_t PIN_NUM_DATA4 = GPIO_NUM_4;   // B4
const gpio_num_t PIN_NUM_DATA5 = GPIO_NUM_9;   // G0
const gpio_num_t PIN_NUM_DATA6 = GPIO_NUM_46;  // G1
const gpio_num_t PIN_NUM_DATA7 = GPIO_NUM_3;   // G2
const gpio_num_t PIN_NUM_DATA8 = GPIO_NUM_8;   // G3
const gpio_num_t PIN_NUM_DATA9 = GPIO_NUM_16;  // G4
const gpio_num_t PIN_NUM_DATA10 = GPIO_NUM_1;  // G5
const gpio_num_t PIN_NUM_DATA11 = GPIO_NUM_14; // R0
const gpio_num_t PIN_NUM_DATA12 = GPIO_NUM_21; // R1
const gpio_num_t PIN_NUM_DATA13 = GPIO_NUM_47; // R2
const gpio_num_t PIN_NUM_DATA14 = GPIO_NUM_48; // R3
const gpio_num_t PIN_NUM_DATA15 = GPIO_NUM_45; // R4
const gpio_num_t PIN_NUM_DISP_EN = GPIO_NUM_NC;

const gpio_num_t TOUCH_GT911_SCL = GPIO_NUM_20;
const gpio_num_t TOUCH_GT911_SDA = GPIO_NUM_19;
const gpio_num_t TOUCH_GT911_INT = GPIO_NUM_NC;
const gpio_num_t TOUCH_GT911_RST = GPIO_NUM_38;

audio_board_handle_t audio_board_init(void)
{
    if (board_handle)
    {
        ESP_LOGW(TAG, "The board has already been initialized!");
        return board_handle;
    }
    board_handle = (audio_board_handle_t)audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
    return board_handle;
}

esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set)
{
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = -1,
        .mode = SD_MODE_SPI,
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_err_t ret = esp_periph_start(set, sdcard_handle);
    int retry_time = SDCARD_OPEN_FILE_NUM_MAX;
    bool mount_flag = false;
    while (retry_time--)
    {
        if (periph_sdcard_is_mounted(sdcard_handle))
        {
            mount_flag = true;
            break;
        }
        else
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    if (mount_flag == false)
    {
        ESP_LOGE(TAG, "SD card mount failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SD card mounted");
    return ret;
}

esp_lcd_handle_t audio_board_lcd_init(void)
{
    ESP_LOGI(TAG, "Init backlight and turn on LCD backlight");

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings =
            {
                .pclk_hz = LCD_PIXEL_CLOCK_HZ,
                .h_res = LCD_H_RES,
                .v_res = LCD_V_RES,
                .hsync_pulse_width = 30,
                .hsync_back_porch = 16,
                .hsync_front_porch = 210,
                .vsync_pulse_width = 13,
                .vsync_back_porch = 10,
                .vsync_front_porch = 22,
                .flags =
                    {
                        .hsync_idle_low = (uint32_t)NULL,
                        .vsync_idle_low = (uint32_t)NULL,
                        .de_idle_high = (uint32_t)NULL,
                        .pclk_active_neg = true,
                        .pclk_idle_high = (uint32_t)NULL,
                    },
            },
        .data_width = 16,
        .bits_per_pixel = (uint8_t)NULL,
        .num_fbs = 2,
        .bounce_buffer_size_px = (size_t)NULL,
        .sram_trans_align = (size_t)NULL,
        .psram_trans_align = 64,
        .hsync_gpio_num = PIN_NUM_HSYNC,
        .vsync_gpio_num = PIN_NUM_VSYNC,
        .de_gpio_num = PIN_NUM_DE,
        .pclk_gpio_num = PIN_NUM_PCLK,
        .disp_gpio_num = PIN_NUM_DISP_EN,
        .data_gpio_nums = {
            PIN_NUM_DATA0,
            PIN_NUM_DATA1,
            PIN_NUM_DATA2,
            PIN_NUM_DATA3,
            PIN_NUM_DATA4,
            PIN_NUM_DATA5,
            PIN_NUM_DATA6,
            PIN_NUM_DATA7,
            PIN_NUM_DATA8,
            PIN_NUM_DATA9,
            PIN_NUM_DATA10,
            PIN_NUM_DATA11,
            PIN_NUM_DATA12,
            PIN_NUM_DATA13,
            PIN_NUM_DATA14,
            PIN_NUM_DATA15,
        },
        .flags = {
            .disp_active_low = (uint32_t)NULL,
            .refresh_on_demand = (uint32_t)NULL,
            .fb_in_psram = true,
            .double_fb = (uint32_t)NULL,
            .no_fb = (uint32_t)NULL,
            .bb_invalidate_cache = (uint32_t)NULL,
        },
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize touch controller");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_GT911_SDA,
        .scl_io_num = TOUCH_GT911_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 100000,
        },
        .clk_flags = 0,
    };
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_GT911_RST,
        .int_gpio_num = TOUCH_GT911_INT,
        .levels = {
            .reset = NULL,
            .interrupt = NULL,
        },
        .flags = {
            .swap_xy = LCD_SWAP_XY ? 1 : 0,
            .mirror_x = LCD_MIRROR_X ? 1 : 0,
            .mirror_y = LCD_MIRROR_Y ? 1 : 0,
        },
        .process_coordinates = NULL,
        .interrupt_callback = NULL,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &tp_io_config, &io_handle));
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &touch_handle));

    const esp_lcd_handle_t esp_lcd = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .touch_handle = touch_handle,
    };

    return esp_lcd;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    audio_free(audio_board);
    board_handle = NULL;
    return ESP_OK;
}
