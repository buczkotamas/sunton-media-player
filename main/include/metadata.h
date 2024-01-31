#ifndef DLNA_METADATA_H
#define DLNA_METADATA_H

#include "esp_err.h"

#define EMPTY_METADATA()     \
    {                        \
        .duration = 0,       \
        .title = NULL,       \
        .creator = NULL,     \
        .album = NULL,       \
        .artist = NULL,      \
        .queueItemId = NULL, \
        .albumArtURI = NULL, \
        .upnp_class = NULL,  \
        .mimeType = NULL,    \
        .guide_id = NULL,    \
        .stream_url = NULL,  \
        .subtitle = NULL,    \
        .description = NULL, \
    }

typedef struct
{
    char *title;
    char *creator;
    char *album;
    char *artist;
    int duration;
    char *queueItemId;
    char *albumArtURI;
    char *upnp_class;
    char *mimeType;
    char *res;

    char *guide_id;
    char *stream_url;
    char *subtitle;
    char *description;

} audio_metadata_t;

esp_err_t metadata_from_dlna_xml(char *xml, audio_metadata_t *metadata);
esp_err_t metadata_to_dlna_xml(audio_metadata_t *metadata, char **xml);

esp_err_t metadata_from_icy_string(char *icy_str, audio_metadata_t *metadata);

void metadata_copy(audio_metadata_t *source, audio_metadata_t *target);
void metadata_free(audio_metadata_t *metadata);

#endif