#ifndef DLNA_METADATA_H
#define DLNA_METADATA_H

#include "esp_err.h"


typedef struct
{
    char *title;
    char *album;
    char *artist;
    int duration;
    char *stream_url;
    char *image_url;
} audio_metadata_t;

typedef enum
{
    METADATA_EVENT = 0,
} metadata_event_t;

typedef void (*metadata_event_cb)(metadata_event_t event, void *subject);

typedef struct
{
    metadata_event_cb callback;
    struct metadataa_event_cb_node_t *next;
} metadata_event_cb_node_t;

void metadata_add_event_listener(metadata_event_cb callback);

esp_err_t metadata_set_dlna_xml(char *xml);
esp_err_t metadata_get_dlna_xml(char **xml);

esp_err_t metadata_set_icy_str(char *icy);

char *metadata_title_get(void);
char *metadata_album_get(void);
char *metadata_artist_get(void);
int metadata_duration_get(void);
char *metadata_stream_url_get(void);
char *metadata_image_url_get(cvoid);

void metadata_set(char *title,
                  char *album,
                  char *artist,
                  int duration,
                  char *stream_url,
                  char *image_url);

#endif