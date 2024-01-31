#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_

/**
 * @brief SDCARD Function Definition
 */
#define SD_MOUNT_POINT "/sdcard"
#define SDCARD_OPEN_FILE_NUM_MAX    5
#define SDCARD_INTR_GPIO            -1
#define SDCARD_PWR_CTRL             -1

#define SD_PIN_NUM_MISO  GPIO_NUM_13
#define SD_PIN_NUM_MOSI  GPIO_NUM_11
#define SD_PIN_NUM_CLK   GPIO_NUM_12
#define SD_PIN_NUM_CS    GPIO_NUM_10

#define SOC_SDMMC_USE_GPIO_MATRIX (false)
#define ESP_SD_PIN_D0    SD_PIN_NUM_MISO  
#define ESP_SD_PIN_CMD   SD_PIN_NUM_MOSI
#define ESP_SD_PIN_CLK   SD_PIN_NUM_CLK
#define ESP_SD_PIN_D3    SD_PIN_NUM_CS

/**
 * @brief Audio Codec Chip Function Definition
 */
#define FUNC_AUDIO_CODEC_EN     (0)
#define BOARD_PA_GAIN           (10) /* Power amplifier gain defined by board (dB) */
#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_I2S_SAMPLE_RATE   AUDIO_SAMPLE_RATE
#define AUDIO_RESAMPLE_RATE     AUDIO_SAMPLE_RATE
#define AUDIO_RINGBUFFER_SIZE   (512 * 1024)

#define AUDIO_I2S_OUTPUT_USB_UART   (1)
#ifdef AUDIO_I2S_OUTPUT_USB_UART
    #define I2S_PIN_DATA_OUT    GPIO_NUM_43; // TX
    #define I2S_PIN_WORD_SELECT GPIO_NUM_44; // RX
    #define I2S_PIN_BIT_CLOCK   GPIO_NUM_18;
#else
    #define I2S_PIN_DATA_OUT    GPIO_NUM_17;
    #define I2S_PIN_WORD_SELECT GPIO_NUM_18;
    #define I2S_PIN_BIT_CLOCK   GPIO_NUM_0; // boot button
#endif

/**
 * @brief LCD SCREEN Function Definition
 */
#define FUNC_LCD_SCREEN_EN          (0)
#define LCD_H_RES                   800
#define LCD_V_RES                   480
#define LCD_SWAP_XY                 (false)
#define LCD_MIRROR_X                (false)
#define LCD_MIRROR_Y                (false)
#define LCD_COLOR_INV               (false)

#define LCD_BACKLIGHT_CHANNEL       0 /* LEDC_CHANNEL_0  */
#define LCD_BACKLIGHT_RESOLUTION    8 /* LEDC_TIMER_8_BIT 0- 255 */
#define LCD_BACKLIGHT_GPIO          2 /* GPIO_NUM_2 */
#define LCD_BACKLIGHT_FREQ_HZ       200
/**
 * @brief LVGL
 */
#define LV_DISPLAY_BUFFER_SIZE      LCD_H_RES * LCD_V_RES
#define LV_DISPLAY_DOUBLE_BUFFER    (false)
#define LV_DISPLAY_BUFFER_DMA       (false)
#define LV_DISPLAY_BUFFER_SPIRAM    (true)

#endif
