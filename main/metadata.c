#include "metadata.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "METADATA";

esp_err_t metadata_from_icy_string(char *icy_str, audio_metadata_t *metadata)
{
    metadata_free(metadata);
    char *start = strstr(icy_str, "StreamTitle='");
    ESP_RETURN_ON_FALSE(start != NULL, ESP_OK, TAG, "ICY metadata has no StreamTitle information");

    start += strlen("StreamTitle='");
    char *end = strstr(start, ";");
    ESP_RETURN_ON_FALSE(end != NULL, ESP_FAIL, TAG, "ICY metadata StreamTitle has no ending ; char");

    size_t len = end - start - 1;
    ESP_RETURN_ON_FALSE(len > 0, ESP_OK, TAG, "ICY metadata StreamTitle is empty");

    char *title = (char *)malloc(len + 1);
    ESP_RETURN_ON_FALSE(title != NULL, ESP_ERR_NO_MEM, TAG, "Cannot allocate memmory for StreamTitle");

    strncpy(title, start, len);
    title[len] = '\0';
    metadata->title = title;
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

esp_err_t metadata_from_dlna_xml(char *xml, audio_metadata_t *metadata)
{
    metadata_free(metadata);
    char *buffer = NULL;
    get_xml_tag_value(xml, &buffer, "upnp:duration");
    if (buffer != NULL)
    {
        metadata->duration = atoi(buffer);
        free(buffer);
    }
    get_xml_tag_value(xml, &(metadata->title), "dc:title");
    get_xml_tag_value(xml, &(metadata->creator), "dc:creator");
    get_xml_tag_value(xml, &(metadata->album), "upnp:album");
    get_xml_tag_value(xml, &(metadata->artist), "upnp:artist");
    get_xml_tag_value(xml, &(metadata->queueItemId), "dc:queueItemId");
    get_xml_tag_value(xml, &(metadata->albumArtURI), "upnp:albumArtURI");
    get_xml_tag_value(xml, &(metadata->upnp_class), "upnp:class");
    get_xml_tag_value(xml, &(metadata->mimeType), "upnp:mimeType");
    return ESP_OK;
}

void metadata_copy(audio_metadata_t *source, audio_metadata_t *target)
{
    metadata_free(target);
    target->duration = source->duration;
    if (source->title != NULL)
        target->title = strdup(source->title);
    if (source->creator != NULL)
        target->creator = strdup(source->creator);
    if (source->album != NULL)
        target->album = strdup(source->album);
    if (source->artist != NULL)
        target->artist = strdup(source->artist);
    if (source->queueItemId != NULL)
        target->queueItemId = strdup(source->queueItemId);
    if (source->albumArtURI != NULL)
        target->albumArtURI = strdup(source->albumArtURI);
    if (source->upnp_class != NULL)
        target->upnp_class = strdup(source->upnp_class);
    if (source->mimeType != NULL)
        target->mimeType = strdup(source->mimeType);
    if (source->guide_id != NULL)
        target->guide_id = strdup(source->guide_id);
    if (source->stream_url != NULL)
        target->stream_url = strdup(source->stream_url);
    if (source->subtitle != NULL)
        target->subtitle = strdup(source->subtitle);
    if (source->description != NULL)
        target->description = strdup(source->description);
}

void metadata_free(audio_metadata_t *metadata)
{
    if (metadata->title != NULL)
        free(metadata->title);
    if (metadata->creator != NULL)
        free(metadata->creator);
    if (metadata->album != NULL)
        free(metadata->album);
    if (metadata->artist != NULL)
        free(metadata->artist);
    if (metadata->queueItemId != NULL)
        free(metadata->queueItemId);
    if (metadata->albumArtURI != NULL)
        free(metadata->albumArtURI);
    if (metadata->upnp_class != NULL)
        free(metadata->upnp_class);
    if (metadata->mimeType != NULL)
        free(metadata->mimeType);
    if (metadata->guide_id != NULL)
        free(metadata->guide_id);
    if (metadata->stream_url != NULL)
        free(metadata->stream_url);
    if (metadata->subtitle != NULL)
        free(metadata->subtitle);
    if (metadata->description != NULL)
        free(metadata->description);

    metadata->duration = 0;
    metadata->title = NULL;
    metadata->creator = NULL;
    metadata->album = NULL;
    metadata->artist = NULL;
    metadata->queueItemId = NULL;
    metadata->albumArtURI = NULL;
    metadata->upnp_class = NULL;
    metadata->mimeType = NULL;
    metadata->guide_id = NULL;
    metadata->stream_url = NULL;
    metadata->subtitle = NULL;
    metadata->description = NULL;
}

static const char *DIDL_START = "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" xmlns : dlna = \"urn:schemas-dlna-org:metadata-1-0/\"><item id=\"1\" parentID=\"0\" restricted=\"1\">";
static const char *DIDL_END = "</item></DIDL-Lite>";
static const char *DIDL_TITLE = "<dc:title>%s</dc:title>";
static const char *DIDL_CREATOR = "<dc:creator>%s</dc:creator>";
static const char *DIDL_ALBUM = "<upnp:album>%s</upnp:album>";
static const char *DIDL_ARTIST = "<upnp:artist>%s</upnp:artist>";
static const char *DIDL_DURATION = "<upnp:duration>%s</upnp:duration>";
static const char *DIDL_QIID = "<dc:queueItemId>%s</dc:queueItemId>";
static const char *DIDL_ART_URI = "<upnp:albumArtURI>%s</upnp:albumArtURI>";
static const char *DIDL_CLASS = "<upnp:class>%s</upnp:class>";
static const char *DIDL_MIME = "<upnp:mimeType>%s</upnp:mimeType>";
static const char *DIDL_RES = "<res>%s</res>";

esp_err_t metadata_to_dlna_xml(audio_metadata_t *metadata, char **xml)
{
    *xml = NULL;
    int len = strlen(DIDL_START);
    if (metadata->title != NULL)
        len += strlen(metadata->title) + strlen(DIDL_TITLE);
    if (metadata->creator != NULL)
        len += strlen(metadata->creator) + strlen(DIDL_CREATOR);
    if (metadata->album != NULL)
        len += strlen(metadata->album) + strlen(DIDL_ALBUM);
    if (metadata->artist != NULL)
        len += strlen(metadata->artist) + strlen(DIDL_ARTIST);
    // duration
    if (metadata->queueItemId != NULL)
        len += strlen(metadata->queueItemId) + strlen(DIDL_QIID);
    if (metadata->albumArtURI != NULL)
        len += strlen(metadata->albumArtURI) + strlen(DIDL_ART_URI);
    if (metadata->upnp_class != NULL)
        len += strlen(metadata->upnp_class) + strlen(DIDL_CLASS);
    if (metadata->mimeType != NULL)
        len += strlen(metadata->mimeType) + strlen(DIDL_MIME);
    len += strlen(DIDL_END);

    char *buff = (char *)malloc(len);
    if (buff == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate %d bytes of memmory for DIDL XML", len);
        return ESP_FAIL;
    }

    len = 0;
    len += sprintf(buff, "%s", DIDL_START);
    if (metadata->title != NULL)
        len += sprintf(buff + len, DIDL_TITLE, metadata->title);
    if (metadata->creator != NULL)
        len += sprintf(buff + len, DIDL_CREATOR, metadata->creator);
    if (metadata->album != NULL)
        len += sprintf(buff + len, DIDL_ALBUM, metadata->album);
    if (metadata->artist != NULL)
        len += sprintf(buff + len, DIDL_ARTIST, metadata->artist);
    // duration
    if (metadata->queueItemId != NULL)
        len += sprintf(buff + len, DIDL_QIID, metadata->queueItemId);
    if (metadata->albumArtURI != NULL)
        len += sprintf(buff + len, DIDL_ART_URI, metadata->albumArtURI);
    if (metadata->upnp_class != NULL)
        len += sprintf(buff + len, DIDL_CLASS, metadata->upnp_class);
    if (metadata->mimeType != NULL)
        len += sprintf(buff + len, DIDL_MIME, metadata->mimeType);
    len += sprintf(buff + len, "%s", DIDL_END);

    // ESP_LOGD(TAG, "DIDL XML from metadata length = %d xml = %s", len, buff);

    *xml = buff;

    return ESP_OK;
}