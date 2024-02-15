#ifndef PLAYER_H
#define PLAYER_H

#include "esp_audio.h"
#include "esp_peripherals.h"
#include "esp_err.h"

#include "metadata.h"

extern const char* tone_uri[];

typedef enum
{
    MP_STATE_NO_MEDIA = 0,
    MP_STATE_PLAYING = 1,
    MP_STATE_PAUSED = 2,
    MP_STATE_STOPPED = 3,
    MP_STATE_FINISHED = 4,
    MP_STATE_TRANSITIONING = 5,
    MP_STATE_ERROR = 6,
} player_state_t;

typedef enum
{
    MP_EVENT_STATE = 0,
    MP_EVENT_VOLUME = 1,
    MP_EVENT_MUTE = 2,
    MP_EVENT_SOURCE = 3,
    MP_EVENT_POSITION = 4,
    // MP_EVENT_ICY_METADATA = 5,
} player_event_t;

typedef enum
{
    MP_SOURCE_TYPE_NULL = 0,
    MP_SOURCE_TYPE_DLNA = 1,
    MP_SOURCE_TYPE_TUNE_IN = 2,
    MP_SOURCE_TYPE_SD_CARD = 3,
    MP_SOURCE_TYPE_OTHER = 4,
} media_sourece_type_t;

typedef struct
{
    media_sourece_type_t type;
    char * url;
}
media_sourece_t;

#define NULL_MEDIA_SOURCE()          \
    {                                \
        .type = MP_SOURCE_TYPE_NULL, \
        .url = NULL,                 \
    }

typedef void (*event_cb)(player_event_t event, void *subject);

typedef struct
{
    event_cb callback;
    struct event_cb_node_t *next;
} event_cb_node_t;

void player_add_event_listener(event_cb callback);

esp_audio_handle_t player_init(void);
void player_source_set(media_sourece_t *source);
media_sourece_t *player_source_get(void);

void player_play(void);
void player_stop(void);
void player_pause(void);
void player_seek(int position);

player_state_t player_state_get(void);

esp_err_t player_volume_get(int *volume);
esp_err_t player_volume_set(int volume);
bool player_mute_get(void);
esp_err_t player_mute_set(bool mute);

esp_err_t player_audio_duration_get(int *sec);
esp_err_t player_audio_time_get(int *sec);
esp_err_t player_audio_position_get(int *position);

#endif