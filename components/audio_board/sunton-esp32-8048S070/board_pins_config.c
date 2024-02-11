#include "esp_log.h"
#include "audio_error.h"
#include "driver/gpio.h"
#include <string.h>
#include "board_pins_config.h"
#include "board_def.h"
#include "nvs.h"

static const char *TAG = "BOARD_PIN_CONFIG";

esp_err_t get_i2s_pins(i2s_port_t port, board_i2s_pin_t *i2s_config)
{
    uint8_t i2s_output = 0;
    nvs_handle_t nvs_handle;
    if (nvs_open("config", NVS_READWRITE, &nvs_handle) == ESP_OK)
    {
        nvs_get_u8(nvs_handle, "i2s_output", &i2s_output);
    }
    
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == I2S_NUM_0)
    {
        if (i2s_output == 0)
        {
            i2s_config->bck_io_num = I2S_PIN_BUILTIN_BIT_CLOCK;
            i2s_config->ws_io_num = I2S_PIN_BUILTIN_WORD_SELECT;
            i2s_config->data_out_num = I2S_PIN_BUILTIN_DATA_OUT;
        }
        else if (i2s_output == 1)
        {
            i2s_config->bck_io_num = I2S_PIN_UART_BIT_CLOCK;
            i2s_config->ws_io_num = I2S_PIN_UART_WORD_SELECT;
            i2s_config->data_out_num = I2S_PIN_UART_DATA_OUT;
        }
        else
        {
            i2s_config->bck_io_num = I2S_PIN_NO_CHANGE;
            i2s_config->ws_io_num = I2S_PIN_NO_CHANGE;
            i2s_config->data_out_num = I2S_PIN_NO_CHANGE;
        }
        i2s_config->data_in_num = I2S_PIN_NO_CHANGE;
        i2s_config->mck_io_num = I2S_PIN_NO_CHANGE;
    }
    else if (port == I2S_NUM_1)
    {
        i2s_config->bck_io_num = I2S_PIN_NO_CHANGE;
        i2s_config->ws_io_num = I2S_PIN_NO_CHANGE;
        i2s_config->data_out_num = I2S_PIN_NO_CHANGE;
        i2s_config->data_in_num = I2S_PIN_NO_CHANGE;
        i2s_config->mck_io_num = I2S_PIN_NO_CHANGE;
    }
    else
    {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

int8_t get_sdcard_open_file_num_max(void)
{
    return SDCARD_OPEN_FILE_NUM_MAX;
}
