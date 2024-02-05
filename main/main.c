#include "esp_log.h"
#include "nvs_flash.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "media_lib_adapter.h"
#include "audio_idf_version.h"

#include "metadata.h"
#include "display.h"
#include "dlna.h"
#include "buttons.h"
#include "player.h"

static const char *TAG = "MAIN";

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set(TAG, ESP_LOG_DEBUG);
    // esp_log_level_set("DISPLAY", ESP_LOG_DEBUG);
    // esp_log_level_set("LGFX_DISPLAY", ESP_LOG_DEBUG);
    // esp_log_level_set("METADATA", ESP_LOG_DEBUG);
    // esp_log_level_set("PLAYER", ESP_LOG_DEBUG);
    // esp_log_level_set("BUTTONS", ESP_LOG_DEBUG);
    esp_log_level_set("DLNA", ESP_LOG_DEBUG);
    // esp_log_level_set("IMG_DOWNLOAD", ESP_LOG_DEBUG);
    // esp_log_level_set("LVGL", ESP_LOG_DEBUG);
    // esp_log_level_set("JPEG", ESP_LOG_DEBUG);
    // esp_log_level_set("HTTP_STREAM", ESP_LOG_DEBUG);
    // esp_log_level_set("esp-tls", ESP_LOG_DEBUG);
    // esp_log_level_set("eesp-tls-mbedtls", ESP_LOG_DEBUG);

    media_lib_add_default_adapter();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .ssid = "BRFKlehallgatokocsi01",
        .password = "32202102",
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    audio_board_sdcard_init(set);
    
    esp_audio_handle_t player = player_init();

    ESP_LOGI(TAG, "audio_board_lcd_init...");
    esp_lcd_handle_t esp_lcd = audio_board_lcd_init();
    ESP_ERROR_CHECK(display_lvgl_init(esp_lcd, player));
    display_lvgl_start();

    ESP_LOGI(TAG, "start_dlna...");
    dlna_start();

    buttons_start(set, player);


    ESP_LOGI(TAG, "END");
}
