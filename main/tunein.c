#include "esp_log.h"
#include "esp_check.h"
#include <string.h>

#include "cJSON.h"
#include "tunein.h"
#include "http_client.h"
#include "player.h"

#define HTTP_RESPONSE_MAX_SIZE 128 * 1024

static const char *TAG = "TUNE_IN";
static const char *URL_BASE = "https://opml.radiotime.com/Tune.ashx?id=";
static const char *URL_FAVORITES = "https://api.tunein.com/profiles/me/follows?folderId=f1&filter=favorites&serial=9a451e82-6daf-48cf-abdc-9192fda47a63&partnerId=RadioTime";
static const char *URL_NOW_PLAYING = "https://feed.tunein.com/profiles/%s/nowPlaying";

esp_err_t tunein_stream_url_get(radio_station_t *radio_station)
{
    esp_err_t ret = ESP_OK;
    int http_res_size;
    char *http_response = NULL;
    char *download_url = NULL;
    char *stream_url = NULL;

    ESP_GOTO_ON_FALSE((radio_station->stream_url == NULL), ESP_OK, err, TAG, "Radio station stream url already set");
    ESP_GOTO_ON_FALSE((radio_station->guide_id != NULL), ESP_FAIL, err, TAG, "Radio station has no guide_id");

    int url_length = strlen(URL_BASE) + strlen(radio_station->guide_id) + 1;
    download_url = (char *)malloc(url_length);
    strcpy(download_url, URL_BASE);
    strcat(download_url, radio_station->guide_id);

    ret = http_client_get(download_url, &http_response, &http_res_size, HTTP_RESPONSE_MAX_SIZE);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Cannot download m3u file");

    const char *first_r = strchr(http_response, '\r');
    const char *first_n = strchr(http_response, '\n');

    ESP_GOTO_ON_FALSE((first_n != NULL), ESP_FAIL, err, TAG, "Cannot determine first EOL of m3u file");

    int line_length = first_r != NULL ? first_r - http_response : first_n - http_response;
    stream_url = (char *)malloc(line_length + 1);
    ESP_GOTO_ON_FALSE(stream_url, ESP_ERR_NO_MEM, err, TAG, "Not enough memmory for stream url");

    // cut s from https protocol
    if (stream_url[4] == 's')
    {
        strncpy(stream_url, http_response, 4);
        strncpy(stream_url[5], http_response[5], line_length - 5);
        line_length--;
    }
    else
    {
        strncpy(stream_url, http_response, line_length);
    }
    stream_url[line_length] = 0;

    ESP_LOGI(TAG, "first line: [%s] length: %d", stream_url, line_length);

    radio_station->stream_url = stream_url;

err:
    if (download_url)
    {
        free(download_url);
    }
    if (http_response)
    {
        free(http_response);
    }
    return ret;
}

esp_err_t tunein_favorites_get(radio_station_t **stations, int *count)
{
    esp_err_t ret = ESP_OK;
    int http_res_size;
    char *http_response = NULL;
    cJSON *json_root = NULL;
    *stations = NULL;

    ret = http_client_get(URL_FAVORITES, &http_response, &http_res_size, HTTP_RESPONSE_MAX_SIZE);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Cannot download TuneIn favorites");

    cJSON *json_station = NULL;
    cJSON *json_stations = NULL;
    json_root = cJSON_Parse(http_response);

    cJSON *Header = cJSON_GetObjectItem(json_root, "Header");
    char *Title = cJSON_GetObjectItem(Header, "Title")->valuestring;
    ESP_LOGI(TAG, "TuneIn folder name: %s", Title);

    json_stations = cJSON_GetObjectItemCaseSensitive(json_root, "Items");
    int station_count = cJSON_GetArraySize(json_stations);
    radio_station_t *radio_stations = (radio_station_t *)malloc(sizeof(radio_station_t) * station_count);
    ESP_GOTO_ON_FALSE(radio_stations, ESP_ERR_NO_MEM, err, TAG, "Not enough memmory for radio stations");

    int i = 0;
    cJSON_ArrayForEach(json_station, json_stations)
    {
        cJSON *guide_id = cJSON_GetObjectItemCaseSensitive(json_station, "GuideId");
        cJSON *image_url = cJSON_GetObjectItemCaseSensitive(json_station, "Image");
        cJSON *title = cJSON_GetObjectItemCaseSensitive(json_station, "Title");
        cJSON *subtitle = cJSON_GetObjectItemCaseSensitive(json_station, "Subtitle");
        cJSON *description = cJSON_GetObjectItemCaseSensitive(json_station, "Description");

        radio_stations[i].guide_id = NULL;
        radio_stations[i].stream_url = NULL;
        radio_stations[i].image_url = NULL;
        radio_stations[i].title = NULL;
        radio_stations[i].subtitle = NULL;
        radio_stations[i].description = NULL;

        if (cJSON_IsString(title) && title->valuestring != NULL)
        {
            radio_stations[i].title = strdup(title->valuestring);
        }
        if (cJSON_IsString(guide_id) && guide_id->valuestring != NULL)
        {
            radio_stations[i].guide_id = strdup(guide_id->valuestring);
        }
        if (cJSON_IsString(subtitle) && subtitle->valuestring != NULL)
        {
            radio_stations[i].subtitle = strdup(subtitle->valuestring);
        }
        if (cJSON_IsString(description) && description->valuestring != NULL)
        {
            radio_stations[i].description = strdup(description->valuestring);
        }
        if (cJSON_IsString(image_url) && image_url->valuestring != NULL)
        {
            radio_stations[i].image_url = strdup(image_url->valuestring);
        }
        i++;
    }

    *count = station_count;
    *stations = radio_stations;

err:
    if (json_root)
    {
        cJSON_Delete(json_root);
    }
    if (http_response)
    {
        free(http_response);
    }
    return ret;
}