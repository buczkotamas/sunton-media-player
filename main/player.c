#include "esp_log.h"
#include "esp_check.h"
#include "esp_audio.h"
#include "esp_decoder.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "fatfs_stream.h"

#include "player.h"
#include "display.h"
#include "board.h"

static const char *TAG = "PLAYER";

static const char *esp_audio_status_names[6] = {"UNKNOWN", "RUNNING", "PAUSED", "STOPPED", "FINISHED", "ERROR"};
static const char *media_sourece_names[6] = {"NULL", "DLNA", "TUNE_IN", "SD_CARD", "OTHER"};

static audio_element_handle_t i2s_stream_handle;
static esp_audio_handle_t player = NULL;
static int muted_volume = 0;
static bool player_muted = false;
static event_cb_node_t *event_cb_list_root = NULL;
static player_state_t player_state = MP_STATE_NO_MEDIA;
static media_sourece_t media_sourece = NULL_MEDIA_SOURCE();

// int test_vol_subject;

void player_add_event_listener(event_cb callback)
{
    if (event_cb_list_root == NULL)
    {
        event_cb_list_root = (event_cb_node_t *)malloc(sizeof(event_cb_node_t));
        event_cb_list_root->callback = callback;
        event_cb_list_root->next = NULL;
    }
    else
    {
        event_cb_node_t *new_node = (event_cb_node_t *)malloc(sizeof(event_cb_node_t));
        new_node->callback = callback;
        new_node->next = NULL;

        event_cb_node_t *node = event_cb_list_root;
        while (node->next != NULL)
            node = node->next;
        node->next = new_node;
    }
}

static void fire_event(player_event_t event, void *subject)
{
    ESP_LOGI(TAG, "Sending event %d to all observer", event);
    event_cb_node_t *node = event_cb_list_root;
    while (node != NULL)
    {
        ESP_LOGI(TAG, "Sending event %d to %p", event, node->callback);
        (node->callback)(event, subject);
        node = node->next;
    }
    ESP_LOGI(TAG, "All observer notified");
}

void player_source_set(media_sourece_t *source)
{
    ESP_LOGD(TAG, "Set URL = %s, source = %s", source->url, media_sourece_names[source->type]);
    esp_audio_stop(player, TERMINATION_TYPE_NOW);
    media_sourece.type = source->type;
    if (media_sourece.url)
    {
        free(media_sourece.url);
    }
    media_sourece.url = strdup(source->url);
    fire_event(MP_EVENT_SOURCE, &media_sourece);
}

media_sourece_t *player_source_get(void)
{
    return &media_sourece;
}

void player_stop(void)
{
    ESP_LOGD(TAG, "Stop");
    esp_audio_stop(player, TERMINATION_TYPE_NOW);
}

void player_pause(void)
{

    ESP_LOGD(TAG, "Pause");
    esp_audio_pause(player);
}

/* seek to position seconds */
void player_seek(int position)
{
    ESP_LOGD(TAG, "Seek to %d", position);
    if (esp_audio_seek(player, position) == ESP_OK)
    {
        fire_event(MP_EVENT_POSITION, &position);
    }
}

player_state_t player_state_get(void)
{
    return player_state;
}

void player_play(void)
{
    esp_audio_state_t state = {0};
    esp_audio_state_get(player, &state);
    if (state.status == AUDIO_STATUS_PAUSED)
    {
        ESP_LOGD(TAG, "Resume");
        esp_audio_resume(player);
    }
    else if (media_sourece.url != NULL)
    {
        ESP_LOGD(TAG, "Playing %s", media_sourece.url);
        if (state.status != AUDIO_STATUS_RUNNING)
        {
            player_state = MP_STATE_TRANSITIONING;
            fire_event(MP_EVENT_STATE, &player_state);
        }
        esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, media_sourece.url, 0);
    }
    else
    {
        ESP_LOGW(TAG, "Play command recieved but no streaming URL set");
    }
}

static esp_err_t i2s_stream_volume_set(audio_element_handle_t i2s_stream, int volume)
{
    ESP_LOGD(TAG, "Volume set = %d", volume);
    if (i2s_alc_volume_set(i2s_stream, volume * 1.28 - 64) == ESP_OK)
    {
        fire_event(MP_EVENT_VOLUME, &volume);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t player_volume_set(int volume)
{
    return i2s_stream_volume_set(i2s_stream_handle, volume);
}

static esp_err_t i2s_stream_volume_get(audio_element_handle_t i2s_stream, int *volume)
{
    ESP_LOGD(TAG, "Volume get");
    int i2s_vol = 0;
    if (i2s_alc_volume_get(i2s_stream, &i2s_vol) == ESP_OK)
    {
        *volume = (i2s_vol + 64) / 1.28;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t player_volume_get(int *volume)
{
    return i2s_stream_volume_get(i2s_stream_handle, volume);
}

bool player_mute_get(void)
{
    return player_muted;
}

esp_err_t player_mute_set(bool mute)
{
    esp_err_t ret = ESP_OK;
    if (mute)
    {
        ESP_GOTO_ON_ERROR(player_volume_get(&muted_volume), err, TAG, "Cannot get volume before muting");
        ESP_GOTO_ON_ERROR(player_volume_set(0), err, TAG, "Cannot set volume to 0 for muting");
    }
    else
    {
        ESP_GOTO_ON_ERROR(player_volume_set(muted_volume), err, TAG, "Cannot restore volume for unmuting");
    }
    player_muted = mute;
    fire_event(MP_EVENT_MUTE, &mute);
err:
    return ret;
}

esp_err_t player_audio_time_get(int *sec)
{
    int time = 0;
    esp_audio_time_get(player, &time);
    *sec = time / 1000;
    return ESP_OK;
}

esp_err_t player_audio_duration_get(int *sec)
{
    int duration = 0;
    esp_audio_duration_get(player, &duration);
    duration /= 1000;
    *sec = duration;
    return ESP_OK;
}

esp_err_t player_audio_position_get(int *position)
{
    return esp_audio_pos_get(player, position);
}

static void esp_audio_callback(esp_audio_state_t *state, void *ctx)
{
    ESP_LOGD(TAG, "Audio event recieved, status = %s", esp_audio_status_names[state->status]);
    switch (state->status)
    {
    case AUDIO_STATUS_RUNNING:
        player_state = MP_STATE_PLAYING;
        break;
    case AUDIO_STATUS_PAUSED:
        player_state = MP_STATE_PAUSED;
        break;
    case AUDIO_STATUS_STOPPED:
        player_state = MP_STATE_STOPPED;
        break;
    case AUDIO_STATUS_FINISHED:
        player_state = MP_STATE_FINISHED;
        break;
    case AUDIO_STATUS_ERROR:
        player_state = MP_STATE_ERROR;
        break;
    default:
        player_state = MP_STATE_TRANSITIONING;
        break;
    }
    fire_event(MP_EVENT_STATE, &player_state);
}

static int esp_http_stream_callback(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS)
    {
        return ESP_OK;
    }
    if (msg->event_id == HTTP_STREAM_FINISH_TRACK)
    {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST)
    {
        return http_stream_restart(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_ICY_METADATA)
    {
        ESP_LOGI(TAG, "ICY metadata found in http stream[%d] = [%s]", msg->buffer_len, (char *)msg->buffer);
        audio_metadata_t metadata = EMPTY_METADATA();
        metadata_from_icy_string(msg->buffer, &metadata);
        fire_event(MP_EVENT_ICY_METADATA, &metadata);
    }
    if (msg->event_id == HTTP_STREAM_ICY_HEADER)
    {
        ESP_LOGI(TAG, "ICY header found in http stream[%d] = [%s]", msg->buffer_len, (char *)msg->buffer);
        //fire_event(MP_EVENT_ICY_HEADER, msg->buffer);
    }    
    return ESP_OK;
}

esp_audio_handle_t player_init(void)
{
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    i2s_writer.stack_in_ext = true;
    i2s_writer.i2s_config.sample_rate = AUDIO_I2S_SAMPLE_RATE;
    i2s_writer.task_core = 1;
    i2s_writer.use_alc = true;
    i2s_writer.volume = -64;
    i2s_writer.i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_writer.i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_writer.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_writer.i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_writer.i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_writer.i2s_config.dma_buf_count = 8;  // esp-adf = 3
    i2s_writer.i2s_config.dma_buf_len = 1024; // esp-adf = 300
    i2s_writer.i2s_config.use_apll = false;
    i2s_writer.i2s_config.tx_desc_auto_clear = true;
    i2s_writer.i2s_config.fixed_mclk = I2S_PIN_NO_CHANGE;
    i2s_writer.i2s_config.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;
    i2s_writer.out_rb_size = AUDIO_RINGBUFFER_SIZE; // mod
    i2s_writer.buffer_len = 12 * 1024;              // mod
    i2s_writer.task_prio = 4;                       // mod

    i2s_stream_handle = i2s_stream_init(&i2s_writer);
    // audio_board_handle_t board_handle = audio_board_init();

    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();

    cfg.vol_handle = i2s_stream_handle;
    cfg.vol_set = (audio_volume_set)i2s_stream_volume_set;
    cfg.vol_get = (audio_volume_get)i2s_stream_volume_get;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = AUDIO_RESAMPLE_RATE;
    player = esp_audio_create(&cfg);

    // Create readers and add to esp_audio
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = esp_http_stream_callback;
    http_cfg.out_rb_size = AUDIO_RINGBUFFER_SIZE; // esp-adf = HTTP_STREAM_RINGBUFFER_SIZE; // 20*1024
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    http_cfg.stack_in_ext = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    auto_dec_cfg.out_rb_size = AUDIO_RINGBUFFER_SIZE;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, esp_decoder_init(&auto_dec_cfg, auto_decode, 5));

    esp_audio_output_stream_add(player, i2s_stream_handle);

    // SD card
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

    esp_audio_input_stream_add(player, fatfs_stream_reader);

    esp_audio_callback_set(player, esp_audio_callback, NULL);

    // Set default volume
    esp_audio_vol_set(player, 16);

    return player;
}
