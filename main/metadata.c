#include "metadata.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include <stdio.h>
#include <string.h>

#define METADATA_NEW()      \
    {                       \
        .duration = 0,      \
        .title = NULL,      \
        .album = NULL,      \
        .artist = NULL,     \
        .stream_url = NULL, \
        .image_url = NULL,  \
    }

static const char *TAG = "METADATA";
audio_metadata_t metadata = METADATA_NEW();
static metadata_event_cb_node_t *event_cb_list_root = NULL;

void metadata_add_event_listener(metadata_event_cb callback)
{
    if (event_cb_list_root == NULL)
    {
        event_cb_list_root = (metadata_event_cb_node_t *)malloc(sizeof(metadata_event_cb_node_t));
        event_cb_list_root->callback = callback;
        event_cb_list_root->next = NULL;
    }
    else
    {
        metadata_event_cb_node_t *new_node = (metadata_event_cb_node_t *)malloc(sizeof(metadata_event_cb_node_t));
        new_node->callback = callback;
        new_node->next = NULL;

        metadata_event_cb_node_t *node = event_cb_list_root;
        while (node->next != NULL)
            node = node->next;
        node->next = new_node;
    }
}

static void fire_event(metadata_event_t event, void *subject)
{
    ESP_LOGD(TAG, "Sending event %d to all observer", event);
    metadata_event_cb_node_t *node = event_cb_list_root;
    while (node != NULL)
    {
        ESP_LOGI(TAG, "Sending event %d to %p", event, node->callback);
        (node->callback)(event, subject);
        node = node->next;
    }
    ESP_LOGI(TAG, "All observer notified");
}

esp_err_t metadata_set_icy_str(char *icy)
{
    char *start = strstr(icy, "StreamTitle='");
    ESP_RETURN_ON_FALSE(start != NULL, ESP_OK, TAG, "ICY metadata has no StreamTitle information");
    start += strlen("StreamTitle='");
    char *end = strstr(start, ";");
    ESP_RETURN_ON_FALSE(end != NULL, ESP_FAIL, TAG, "ICY metadata StreamTitle has no ending ; char");
    size_t len = end - start - 1;
    ESP_RETURN_ON_FALSE(len > 0, ESP_OK, TAG, "ICY metadata StreamTitle is empty");
    if (metadata.title != NULL)
        free(metadata.title);
    metadata.title = (char *)malloc(len + 1);
    ESP_RETURN_ON_FALSE(metadata.title != NULL, ESP_ERR_NO_MEM, TAG, "Cannot allocate memmory for metadata.title");
    strncpy(metadata.title, start, len);
    metadata.title[len] = '\0';
    fire_event(METADATA_EVENT, &metadata);
    return ESP_OK;
}

static void get_xml_tag_value(char *xml, char **value, char *key)
{
    *value = NULL;
    char start_tag[24];
    char end_tag[24];
    snprintf(start_tag, sizeof(start_tag), "<%s>", key);
    snprintf(end_tag, sizeof(end_tag), "</%s>", key);
    char *start = strstr(xml, start_tag);
    char *end = strstr(xml, end_tag);
    if (start == NULL || end == NULL)
    {
        ESP_LOGD(TAG, "%s is NULL", key);
        return;
    }
    start += strlen(start_tag);
    size_t len = end - start;
    if (len == 0)
    {
        ESP_LOGD(TAG, "%s is empty", key);
        return;
    }

    char *_value = (char *)malloc(len + 1);
    if (_value == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate memmory for key %s", key);
        return;
    }
    strncpy(_value, start, len);
    _value[len] = '\0';
    *value = _value;

    ESP_LOGD(TAG, "%s = %s, len = %d", key, _value, len);
}

static void metadata_free(void)
{
    if (metadata.title != NULL)
        free(metadata.title);
    if (metadata.album != NULL)
        free(metadata.album);
    if (metadata.artist != NULL)
        free(metadata.artist);
    if (metadata.stream_url != NULL)
        free(metadata.stream_url);
    if (metadata.image_url != NULL)
        free(metadata.image_url);
    metadata.duration = 0;
    metadata.title = NULL;
    metadata.album = NULL;
    metadata.artist = NULL;
    metadata.stream_url = NULL;
    metadata.image_url = NULL;
}

esp_err_t metadata_set_dlna_xml(char *xml)
{
    metadata_free();
    char *buffer = NULL;
    get_xml_tag_value(xml, &buffer, "upnp:duration");
    if (buffer != NULL)
    {
        metadata.duration = atoi(buffer);
        free(buffer);
    }
    get_xml_tag_value(xml, &(metadata.title), "dc:title");
    get_xml_tag_value(xml, &(metadata.album), "upnp:album");
    get_xml_tag_value(xml, &(metadata.artist), "upnp:artist");
    get_xml_tag_value(xml, &(metadata.image_url), "upnp:albumArtURI");

    fire_event(METADATA_EVENT, &metadata);

    return ESP_OK;
}

static const char *DIDL_START = "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" xmlns : dlna = \"urn:schemas-dlna-org:metadata-1-0/\"><item id=\"1\" parentID=\"0\" restricted=\"1\">";
static const char *DIDL_TITLE = "<dc:title>%s</dc:title>";
static const char *DIDL_ALBUM = "<upnp:album>%s</upnp:album>";
static const char *DIDL_ARTIST = "<upnp:artist>%s</upnp:artist>";
static const char *DIDL_DURATION = "<upnp:duration>%02d:%02d:%02d</upnp:duration>";
static const char *DIDL_ART_URI = "<upnp:albumArtURI>%s</upnp:albumArtURI>";
static const char *DIDL_CLASS = "<upnp:class>object.item.audioItem.musicTrack</upnp:class>";
static const char *DIDL_RES = "<res>%s</res>";
static const char *DIDL_END = "</item></DIDL-Lite>";

esp_err_t metadata_get_dlna_xml(char **xml)
{
    *xml = NULL;
    int len = strlen(DIDL_START);
    if (metadata.title != NULL)
        len += strlen(metadata.title) + strlen(DIDL_TITLE);
    if (metadata.album != NULL)
        len += strlen(metadata.album) + strlen(DIDL_ALBUM);
    if (metadata.artist != NULL)
        len += strlen(metadata.artist) + strlen(DIDL_ARTIST);
    if (metadata.image_url != NULL)
        len += strlen(metadata.image_url) + strlen(DIDL_ART_URI);
    if (metadata.stream_url != NULL)
        len += strlen(metadata.stream_url) + strlen(DIDL_RES);
    len += strlen(DIDL_DURATION);
    len += strlen(DIDL_CLASS);
    len += strlen(DIDL_END);

    char *buff = (char *)malloc(len);
    if (buff == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate %d bytes of memmory for DIDL XML", len);
        return ESP_FAIL;
    }

    len = 0;
    len += sprintf(buff, "%s", DIDL_START);
    if (metadata.title != NULL)
        len += sprintf(buff + len, DIDL_TITLE, metadata.title);
    if (metadata.album != NULL)
        len += sprintf(buff + len, DIDL_ALBUM, metadata.album);
    if (metadata.artist != NULL)
        len += sprintf(buff + len, DIDL_ARTIST, metadata.artist);
    if (metadata.image_url != NULL)
        len += sprintf(buff + len, DIDL_ART_URI, metadata.image_url);
    if (metadata.stream_url != NULL)
        len += sprintf(buff + len, DIDL_RES, metadata.stream_url);
    len += sprintf(buff + len, DIDL_DURATION, metadata.duration / 3600, metadata.duration / 60, metadata.duration % 60);
    len += sprintf(buff + len, "%s", DIDL_CLASS);
    len += sprintf(buff + len, "%s", DIDL_END);

    ESP_LOGD(TAG, "DIDL XML from metadata length = %d xml = %s", len, buff);

    *xml = buff;

    return ESP_OK;
}

char *metadata_title_get(void) { return metadata.title; }
char *metadata_album_get(void) { return metadata.album; }
char *metadata_artist_get(void) { return metadata.artist; }
int metadata_duration_get(void) { return metadata.duration; }
char *metadata_stream_url_get(void) { return metadata.stream_url; }
char *metadata_image_url_get(void) { return metadata.image_url; }

void metadata_set(char *title,
                  char *album,
                  char *artist,
                  int duration,
                  char *stream_url,
                  char *image_url)
{
    metadata_free();
    if (title != NULL)
        metadata.title = strdup(title);
    if (album != NULL)
        metadata.album = strdup(album);
    if (artist != NULL)
        metadata.artist = strdup(artist);
    if (stream_url != NULL)
        metadata.stream_url = strdup(stream_url);
    if (image_url != NULL)
        metadata.image_url = strdup(image_url);
    metadata.duration = duration;
    fire_event(METADATA_EVENT, &metadata);
}