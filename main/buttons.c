#include "buttons.h"
#include "board_def.h"

#include "esp_log.h"
#include "input_key_service.h"

static const char *TAG = "BUTTON";

static esp_audio_handle_t player = NULL;

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    int volume = 0;
    // ESP_LOGD(TAG, "[ * ] input key id is %d, %d", (int)evt->data, evt->type);
    const char *key_types[INPUT_KEY_SERVICE_ACTION_PRESS_RELEASE + 1] = {"UNKNOWN", "CLICKED", "CLICK RELEASED", "PRESSED", "PRESS RELEASED"};
    // ESP_LOGD(TAG, "[ * ] KEY %s", key_types[evt->type]);
    if (evt->type == 1)
    { // CLICKED
        switch ((int)evt->data)
        {
        case INPUT_KEY_USER_ID_PLAY:
            ESP_LOGD(TAG, "[ * ] [Play] KEY %s", key_types[evt->type]);
            esp_audio_state_t state = {0};
            esp_audio_state_get(player, &state);
            if (state.status == AUDIO_STATUS_PAUSED)
            {
                esp_audio_resume(player);
                // esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            }
            else if (state.status == AUDIO_STATUS_RUNNING)
            {
                esp_audio_pause(player);
                // esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            }
            break;
        case INPUT_KEY_USER_ID_VOLDOWN:
            ESP_LOGD(TAG, "[ * ] [Vol-] KEY %s", key_types[evt->type]);
            esp_audio_vol_get(player, &volume);
            volume = volume - 5 < 0 ? 0 : volume - 5;
            esp_audio_vol_set(player, volume);
            break;
        case INPUT_KEY_USER_ID_VOLUP:
            ESP_LOGD(TAG, "[ * ] [Vol+] KEY %s", key_types[evt->type]);
            esp_audio_vol_get(player, &volume);
            volume = volume + 5 > 100 ? 100 : volume + 5;
            esp_audio_vol_set(player, volume);
            break;
        case INPUT_KEY_USER_ID_MUTE:
            ESP_LOGD(TAG, "[ * ] [MUTE] KEY %s", key_types[evt->type]);
            break;
        default:
            ESP_LOGE(TAG, "User Key ID[%d] does not support", (int)evt->data);
            break;
        }
    }
    return ESP_OK;
}

void buttons_start(esp_periph_set_handle_t set, esp_audio_handle_t player_handle)
{
#ifdef FUNC_BUTTON_EN
    player = player_handle;
    audio_board_key_init(set);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)NULL);
#endif
}
