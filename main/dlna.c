#include "dlna.h"
#include "metadata.h"
#include "hescape.h"
#include "player.h"

#include "esp_ssdp.h"
#include "esp_log.h"

static const char *TAG = "DLNA";

static esp_dlna_handle_t dlna_handle;
static char *escaped_metadata = NULL;

static char *player_state_to_trans_state(player_state_t player_state)
{
    switch (player_state)
    {
    case MP_STATE_NO_MEDIA:
        return "NO_MEDIA_PRESENT";
    case MP_STATE_PLAYING:
        return "PLAYING";
    case MP_STATE_PAUSED:
        return "PAUSED_PLAYBACK";
    case MP_STATE_TRANSITIONING:
        return "TRANSITIONING";
    case MP_STATE_STOPPED:
    case MP_STATE_FINISHED:
    case MP_STATE_ERROR:
        return "STOPPED";
    default:
        return "STOPPED";
    }
}

static int dlna_renderer_request(esp_dlna_handle_t dlna, const upnp_attr_t *attr, int attr_num, char *buffer, int max_buffer_len)
{
    int req_type;
    int tmp_data = 0;
    int hour = 0, min = 0, sec = 0, mute, vol;

    if (attr_num != 1)
    {
        return 0;
    }

    req_type = attr->type & 0xFF;
    // ESP_LOGD(TAG, "DLNA req type = %d", req_type);
    switch (req_type)
    {
    case RCS_GET_MUTE:
        mute = player_mute_get() ? 1 : 0;
        ESP_LOGD(TAG, "GetMute, return CurrentMute = %d", mute);
        return snprintf(buffer, max_buffer_len, "%d", mute);
    case RCS_SET_MUTE:
        mute = atoi(buffer);
        ESP_LOGD(TAG, "SetMute, DesiredMute = %d", mute);
        player_mute_set(mute == 0 ? false : true);
        return 0;
    case RCS_GET_VOL:
        player_volume_get(&vol);
        ESP_LOGD(TAG, "GetVolume, return CurrentVolume = %d", vol);
        return snprintf(buffer, max_buffer_len, "%d", vol);
    case RCS_SET_VOL:
        vol = atoi(buffer);
        ESP_LOGD(TAG, "SetVolume, DesiredVolume = %d", vol);
        player_volume_set(vol);
        return 0;
    case AVT_PLAY:
        ESP_LOGD(TAG, "Play");
        player_play();
        return 0;
    case AVT_STOP:
        ESP_LOGD(TAG, "Stop");
        player_stop();
        return 0;
    case AVT_PAUSE:
        ESP_LOGD(TAG, "Pause");
        player_pause();
        return 0;
    case AVT_NEXT:
    case AVT_PREV:
        ESP_LOGD(TAG, "Next/Previous");
        player_stop();
        return 0;
    case AVT_SEEK:
        ESP_LOGD(TAG, "Seek to %s", buffer);
        sscanf(buffer, "%d:%d:%d", &hour, &min, &sec);
        player_seek(hour * 3600 + min * 60 + sec);
        return 0;
    case AVT_SET_TRACK_URI:
        ESP_LOGD(TAG, "SetAVTransportURI, CurrentURI = %s", buffer);
        media_sourece_t source = {
            .type = MP_SOURCE_TYPE_DLNA,
            .url = buffer,
        };
        player_source_set(&source);
        player_play();
        return 0;
    case AVT_SET_TRACK_METADATA:
        ESP_LOGD(TAG, "SetAVTransportURI, CurrentURIMetaData = %s", buffer);
        metadata_set_dlna_xml(buffer);
        if (escaped_metadata != NULL)
            free(escaped_metadata);
        hesc_escape_html((uint8_t **)&escaped_metadata, (uint8_t *)buffer, strlen(buffer));
        return 0;
    case AVT_GET_TRACK_URI:
        char *url = (player_source_get())->url;
        ESP_LOGD(TAG, "GetMediaInfo or GetPositionInfo, CurrentTrackURI or AVTransportURI notify");
        if (url != NULL)
        {
            return snprintf(buffer, max_buffer_len, "%s", url);
        }
        return 0;
    case AVT_GET_PLAY_SPEED:
        return snprintf(buffer, max_buffer_len, "%d", 1);
    case AVT_GET_PLAY_MODE:
        return snprintf(buffer, max_buffer_len, "NORMAL");
    case AVT_GET_TRANS_STATUS:
        ESP_LOGD(TAG, "GetTransportInfo return CurrentTransportStatus = OK (hardcoded)");
        return snprintf(buffer, max_buffer_len, "OK");
    case AVT_GET_TRANS_STATE:
        player_state_t player_state = player_state_get();
        char *trans_state = player_state_to_trans_state(player_state);
        ESP_LOGD(TAG, "GetTransportInfo or TransportState notify, reply = %s", trans_state);
        return snprintf(buffer, max_buffer_len, trans_state);
    case AVT_GET_TRACK_DURATION:
    case AVT_GET_MEDIA_DURATION:
        player_audio_duration_get(&tmp_data);
        if (tmp_data == 0 && metadata_duration_get() != 0)
        {
            tmp_data = metadata_duration_get();
        }
        ESP_LOGD(TAG, "GetMediaInfo or CurrentMediaDuration notify, reply = %02d:%02d:%02d", tmp_data / 3600, tmp_data / 60, tmp_data % 60);
        return snprintf(buffer, max_buffer_len, "%02d:%02d:%02d", tmp_data / 3600, tmp_data / 60, tmp_data % 60);
    case AVT_GET_TRACK_NO:
        return snprintf(buffer, max_buffer_len, "%d", 1);
    case AVT_GET_TRACK_METADATA:
        ESP_LOGD(TAG, "GetMediaInfo / GetPositionInfo or CurrentTrackMetaData / AVTransportURIMetaData notify");
        return escaped_metadata == NULL ? 0 : snprintf(buffer, max_buffer_len, "%s", escaped_metadata);
    case AVT_GET_POS_ABSTIME:
    case AVT_GET_POS_RELTIME:
        player_audio_time_get(&tmp_data);
        ESP_LOGD(TAG, "GetPositionInfo or RelativeTimePosition notify, reply = %02d:%02d:%02d", tmp_data / 3600, tmp_data / 60, tmp_data % 60);
        return snprintf(buffer, max_buffer_len, "%02d:%02d:%02d", tmp_data / 3600, tmp_data / 60, tmp_data % 60);
    case AVT_GET_POS_ABSCOUNT:
    case AVT_GET_POS_RELCOUNT:
        player_audio_position_get(&tmp_data);
        ESP_LOGD(TAG, "GetPositionInfo or RelativeCounterPosition notify, reply = %d", tmp_data);
        return snprintf(buffer, max_buffer_len, "%d", tmp_data);
    }
    return 0;
}

static void metadata_cb(metadata_event_t event, void *subject)
{
    switch (event)
    {
    case METADATA_EVENT:
        if (player_source_get()->type != MP_SOURCE_TYPE_DLNA)
        {
            if (escaped_metadata != NULL)
                free(escaped_metadata);
            char *xml;
            if (metadata_get_dlna_xml(&xml) == ESP_OK)
            {
                ESP_LOGD(TAG, "XML metadata = %s", xml);
                hesc_escape_html((uint8_t **)&escaped_metadata, (uint8_t *)xml, strlen(xml));
                ESP_LOGD(TAG, "Escaped metadata = %s", escaped_metadata);
                if (xml != NULL)
                    free(xml);
            }
        }
        break;
    default:
        break;
    }
}

static void player_cb(player_event_t event, void *subject)
{
    switch (event)
    {
    case MP_EVENT_STATE:
        esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
        break;
    case MP_EVENT_VOLUME:
    case MP_EVENT_MUTE:
        esp_dlna_notify(dlna_handle, "RenderingControl");
        break;
    case MP_EVENT_SOURCE:
        break;
    case MP_EVENT_POSITION:
        esp_dlna_notify_avt_by_action(dlna_handle, "RelativeTimePosition");
        esp_dlna_notify_avt_by_action(dlna_handle, "RelativeCounterPosition");
    default:
        break;
    }
}

esp_dlna_handle_t dlna_start()
{
    ESP_LOGI(TAG, "Starting DLNA...");

    const ssdp_service_t ssdp_service[] = {
        {DLNA_DEVICE_UUID, "upnp:rootdevice", NULL},
        {DLNA_DEVICE_UUID, "urn:schemas-upnp-org:device:MediaRenderer:1", NULL},
        {DLNA_DEVICE_UUID, "urn:schemas-upnp-org:service:ConnectionManager:1", NULL},
        {DLNA_DEVICE_UUID, "urn:schemas-upnp-org:service:RenderingControl:1", NULL},
        {DLNA_DEVICE_UUID, "urn:schemas-upnp-org:service:AVTransport:1", NULL},
        {NULL, NULL, NULL},
    };

    ssdp_config_t ssdp_config = SSDP_DEFAULT_CONFIG();
    ssdp_config.udn = DLNA_UNIQUE_DEVICE_NAME;
    ssdp_config.location = "http://${ip}" DLNA_ROOT_PATH;
    esp_ssdp_start(&ssdp_config, ssdp_service);

    static httpd_handle_t httpd = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.max_uri_handlers = 25;
    httpd_config.stack_size = 8 * 1024;
    if (httpd_start(&httpd, &httpd_config) != ESP_OK)
    {
        ESP_LOGI(TAG, "Error starting httpd");
    }

    extern const uint8_t logo_png_start[] asm("_binary_logo_png_start");
    extern const uint8_t logo_png_end[] asm("_binary_logo_png_end");

    dlna_config_t dlna_config = {
        .friendly_name = "ESP32-S3 DNLA Media Player",
        .uuid = (const char *)DLNA_DEVICE_UUID,
        .logo = {
            .mime_type = "image/png",
            .data = (const char *)logo_png_start,
            .size = logo_png_end - logo_png_start,
        },
        .httpd = httpd,
        .httpd_port = httpd_config.server_port,
        .renderer_req = dlna_renderer_request,
        .root_path = DLNA_ROOT_PATH,
        .device_list = false};

    dlna_handle = esp_dlna_start(&dlna_config);

    ESP_LOGI(TAG, "DLNA started");

    player_add_event_listener(player_cb);
    metadata_add_event_listener(metadata_cb);

    return dlna_handle;
}