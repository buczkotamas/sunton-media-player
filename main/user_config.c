#include "user_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"

#define CONF_NAMESPACE "config"
#define CONF_KEY_I2S_OUT "i2s_output"
#define CONF_KEY_AUDIO_VOL "audio_volume"
#define CONF_KEY_BACKLIGHT_FULL "backlight_full"
#define CONF_KEY_BACKLIGHT_DIMM "backlight_dimm"
#define CONF_KEY_BACKLIGHT_TIMER "backlight_timer"
#define CONF_KEY_SCAN_SD_ON_BOOT "scan_on_boot"

#define CONF_DEF_VAL_I2S_OUT 0
#define CONF_DEF_VAL_AUDIO_VOL 20
#define CONF_DEF_VAL_BACKLIGHT_FULL 255
#define CONF_DEF_VAL_BACKLIGHT_DIMM 32
#define CONF_DEF_VAL_BACKLIGHT_TIMER 1000 * 60 * 5
#define CONF_DEF_VAL_SCAN_SD_ON_BOOT 0

static esp_err_t config_set_u8(char *key, uint8_t value)
{
    nvs_handle_t nvs_handle;
    nvs_open(CONF_NAMESPACE, NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, key, value);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ESP_OK;
}

static uint8_t config_get_u8(char *key, uint8_t default_value)
{
    nvs_handle_t nvs_handle;
    nvs_open(CONF_NAMESPACE, NVS_READWRITE, &nvs_handle);
    uint8_t value = default_value;
    nvs_get_u8(nvs_handle, key, &value);
    return value;
}

static esp_err_t config_set_u32(char *key, uint32_t value)
{
    nvs_handle_t nvs_handle;
    nvs_open(CONF_NAMESPACE, NVS_READWRITE, &nvs_handle);
    nvs_set_u32(nvs_handle, key, value);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ESP_OK;
}

static uint32_t config_get_u32(char *key, uint32_t default_value)
{
    nvs_handle_t nvs_handle;
    nvs_open(CONF_NAMESPACE, NVS_READWRITE, &nvs_handle);
    uint32_t value = default_value;
    nvs_get_u32(nvs_handle, key, &value);
    return value;
}

uint8_t config_get_i2s_output()
{
    return config_get_u8(CONF_KEY_I2S_OUT, CONF_DEF_VAL_I2S_OUT);
}
esp_err_t config_set_i2s_output(uint8_t value)
{
    return config_set_u8(CONF_KEY_I2S_OUT, value);
}

uint8_t config_get_audio_volume()
{
    return config_get_u8(CONF_KEY_AUDIO_VOL, CONF_DEF_VAL_AUDIO_VOL);
}
esp_err_t config_set_audio_volume(uint8_t value)
{
    return config_set_u8(CONF_KEY_AUDIO_VOL, value);
}

uint8_t config_get_backlight_full()
{
    return config_get_u8(CONF_KEY_BACKLIGHT_FULL, CONF_DEF_VAL_BACKLIGHT_FULL);
}
esp_err_t config_set_backlight_full(uint8_t value)
{
    return config_set_u8(CONF_KEY_BACKLIGHT_FULL, value);
}

uint8_t config_get_backlight_dimm()
{
    return config_get_u8(CONF_KEY_BACKLIGHT_DIMM, CONF_DEF_VAL_BACKLIGHT_DIMM);
}
esp_err_t config_set_backlight_dimm(uint8_t value)
{
    return config_set_u8(CONF_KEY_BACKLIGHT_DIMM, value);
}

uint32_t config_get_backlight_timer()
{
    return config_get_u32(CONF_KEY_BACKLIGHT_TIMER, CONF_DEF_VAL_BACKLIGHT_TIMER);
}
esp_err_t config_set_backlight_timer(uint32_t value)
{
    return config_set_u32(CONF_KEY_BACKLIGHT_TIMER, value);
}

bool config_get_scan_card_on_boot()
{
    return config_get_u8(CONF_KEY_SCAN_SD_ON_BOOT, CONF_DEF_VAL_SCAN_SD_ON_BOOT) == 1;
}
esp_err_t config_set_scan_card_on_boot(bool value)
{
    return config_set_u8(CONF_KEY_SCAN_SD_ON_BOOT, value ? 1 : 0);
}