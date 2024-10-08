#include "esp_log.h"
#include "esp_check.h"
#include <string.h>

#include "cJSON.h"
#include "tunein_browser.h"
#include "http_client.h"
#include "player.h"
#include "img_download.h"
#include "msg_window.h"
#include "gui.h"

#define HTTP_RESPONSE_MAX_SIZE 128 * 1024

static const char *TAG = "TUNE_IN";
static const char *URL_BASE = "https://opml.radiotime.com/Tune.ashx?id=";
static const char *URL_FAVORITES = "https://api.tunein.com/profiles/me/follows?folderId=f1&filter=favorites&serial=9a451e82-6daf-48cf-abdc-9192fda47a63&partnerId=RadioTime";
// static const char *URL_NOW_PLAYING = "https://feed.tunein.com/profiles/%s/nowPlaying";

static lv_obj_t *active_station_button = NULL;
static char *active_station_guide_id = NULL;
static lv_obj_t *station_list = NULL;
static radio_station_t *radio_stations = NULL;
static int radio_stations_size = 0;

LV_IMG_DECLARE(radio_128x104);
LV_IMG_DECLARE(tunein_refresh_128x128);

static esp_err_t tunein_stream_url_get(radio_station_t *radio_station)
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

// listen only for LV_EVENT_VALUE_CHANGED
static void station_button_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    bool checked = lv_obj_has_state(target, LV_STATE_CHECKED);
    if (checked)
    {
        msg_window_show_text("Connecting...");
        radio_station_t *radio_station = (radio_station_t *)e->user_data;

        if (active_station_button != NULL)
            lv_obj_clear_state(active_station_button, LV_STATE_CHECKED);
        active_station_button = target;
        if (active_station_guide_id != NULL)
            free(active_station_guide_id);
        active_station_guide_id = strdup(radio_station->guide_id);

        esp_err_t url_ret = tunein_stream_url_get(radio_station);
        if (url_ret == ESP_OK)
        {
            media_sourece_t source = {
                .type = MP_SOURCE_TYPE_TUNE_IN,
                .url = radio_station->stream_url,
            };
            player_source_set(&source);
            metadata_set(radio_station->title, radio_station->title, radio_station->stream_url, 0, radio_station->stream_url, radio_station->image_url);
            audio_err_t play_ret = player_play();
            if (play_ret != ESP_OK)
                msg_window_show_ok("%s  Player error: %d", LV_SYMBOL_WARNING, play_ret);
        }
        else
        {
            msg_window_show_ok("%s  TuneIn get URL error: %d", LV_SYMBOL_WARNING, url_ret);
        }
    }
    else if (active_station_button == target)
    {
        if (active_station_guide_id != NULL)
            free(active_station_guide_id);
        active_station_guide_id = NULL;
        active_station_button = NULL;
        player_stop();
    }
}

static esp_err_t tunein_favorites_get(radio_station_t **stations, int *count)
{
    esp_err_t ret = ESP_OK;
    int http_res_size;
    char *http_response = NULL;
    cJSON *json_root = NULL;
    *stations = NULL;

    if (!heap_caps_check_integrity_all(true))
        ESP_LOGE(TAG, "Heap corruption detected! - tunein_favorites_get - 1");
    else
        ESP_LOGI(TAG, "Heap corruption not detected! - tunein_favorites_get - 1");

    ret = http_client_get(URL_FAVORITES, &http_response, &http_res_size, HTTP_RESPONSE_MAX_SIZE);

    if (!heap_caps_check_integrity_all(true))
        ESP_LOGE(TAG, "Heap corruption detected! - tunein_favorites_get - 2");
    else
        ESP_LOGI(TAG, "Heap corruption not detected! - tunein_favorites_get - 2");

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
            radio_stations[i].title = strdup(title->valuestring);
        if (cJSON_IsString(guide_id) && guide_id->valuestring != NULL)
            radio_stations[i].guide_id = strdup(guide_id->valuestring);
        if (cJSON_IsString(subtitle) && subtitle->valuestring != NULL)
            radio_stations[i].subtitle = strdup(subtitle->valuestring);
        if (cJSON_IsString(description) && description->valuestring != NULL)
            radio_stations[i].description = strdup(description->valuestring);
        if (cJSON_IsString(image_url) && image_url->valuestring != NULL)
            radio_stations[i].image_url = strdup(image_url->valuestring);

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

    if (!heap_caps_check_integrity_all(true))
        ESP_LOGE(TAG, "Heap corruption detected! - tunein_favorites_get - end");
    else
        ESP_LOGI(TAG, "Heap corruption not detected! - tunein_favorites_get - end");

    return ret;
}

#define COL_COUNT 4
#define BUTTON_SIZE 160
#define BUTTON_IMAGE_SIZE 128

static void refresh_button_handler(lv_event_t *e)
{
    tunein_browser_refresh();
}

static void add_refresh_button(int item_count)
{
    if (item_count == 0)
    {
        static lv_coord_t dsc[] = {BUTTON_SIZE, LV_GRID_TEMPLATE_LAST};
        lv_obj_set_style_grid_column_dsc_array(station_list, dsc, 0);
        lv_obj_set_style_grid_row_dsc_array(station_list, dsc, 0);
        lv_obj_set_layout(station_list, LV_LAYOUT_GRID);
    }
    uint8_t col = item_count % COL_COUNT;
    uint8_t row = item_count / COL_COUNT;
    lv_obj_t *button = lv_btn_create(station_list);
    lv_obj_add_event_cb(button, refresh_button_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_grid_cell(button, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_img_src(button, &tunein_refresh_128x128, 0);
}

static void free_radio_station_list()
{
    if (radio_stations != NULL)
    {
        for (int i = 0; i < radio_stations_size; i++)
        {
            if (radio_stations[i].guide_id != NULL)
                free(radio_stations[i].guide_id);
            if (radio_stations[i].stream_url != NULL)
                free(radio_stations[i].stream_url);
            if (radio_stations[i].image_url != NULL)
                free(radio_stations[i].image_url);
            if (radio_stations[i].title != NULL)
                free(radio_stations[i].title);
            if (radio_stations[i].subtitle != NULL)
                free(radio_stations[i].subtitle);
            if (radio_stations[i].description != NULL)
                free(radio_stations[i].description);
        }
        free(radio_stations);
        radio_stations = NULL;
        radio_stations_size = 0;
    }
}

void tunein_browser_refresh(void)
{
    if (!heap_caps_check_integrity_all(true))
        ESP_LOGE(TAG, "Heap corruption detected! - tunein_browser_refresh - 1");
    else
        ESP_LOGI(TAG, "Heap corruption not detected! - tunein_browser_refresh - 1");

    msg_window_show_text("Loading...");
    lv_obj_clean(station_list);
    free_radio_station_list();
    if (tunein_favorites_get(&radio_stations, &radio_stations_size) == ESP_OK)
    {
        lv_coord_t *col_dsc = (lv_coord_t *)malloc((COL_COUNT + 1) * sizeof(lv_coord_t));
        for (int i = 0; i < COL_COUNT; i++)
        {
            col_dsc[i] = BUTTON_SIZE;
        }
        col_dsc[COL_COUNT] = LV_GRID_TEMPLATE_LAST;

        int ROW_COUNT = ((radio_stations_size + 1) / COL_COUNT) + 1;
        lv_coord_t *row_dsc = (lv_coord_t *)malloc((ROW_COUNT + 1) * sizeof(lv_coord_t));
        for (int i = 0; i < ROW_COUNT; i++)
        {
            row_dsc[i] = BUTTON_SIZE;
        }
        row_dsc[ROW_COUNT] = LV_GRID_TEMPLATE_LAST;

        ESP_LOGD(TAG, "radio_stations_size=%d, COL_COUNT=%d, ROW_COUNT=%d", radio_stations_size, COL_COUNT, ROW_COUNT);

        lv_obj_set_style_grid_column_dsc_array(station_list, col_dsc, 0);
        lv_obj_set_style_grid_row_dsc_array(station_list, row_dsc, 0);
        lv_obj_set_layout(station_list, LV_LAYOUT_GRID);

        lv_obj_t *img;
        lv_obj_t *button;
        lv_obj_t *label;
        lv_obj_t *canvas;
        void *canvas_buffer;

        if (!heap_caps_check_integrity_all(true))
            ESP_LOGE(TAG, "Heap corruption detected! - tunein_browser_refresh - 2");
        else
            ESP_LOGI(TAG, "Heap corruption not detected! - tunein_browser_refresh - 2");

        for (int i = 0; i < radio_stations_size; i++)
        {
            msg_window_show_text("Loading %d/%d", radio_stations_size, i + 1);

            uint8_t col = i % COL_COUNT;
            uint8_t row = i / COL_COUNT;

            ESP_LOGD(TAG, "title=%s, i=%d, col=%d, row=%d", radio_stations[i].title, i, col, row);

            button = lv_btn_create(station_list);
            lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
            lv_obj_add_event_cb(button, station_button_handler, LV_EVENT_VALUE_CHANGED, &radio_stations[i]);
            lv_obj_set_grid_cell(button, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
            if (active_station_guide_id != NULL && strcmp(active_station_guide_id, radio_stations[i].guide_id) == 0)
            {
                lv_obj_add_state(button, LV_STATE_CHECKED);
            }

            lv_img_dsc_t station_image = {
                .data = NULL,
            };
            if (download_image(radio_stations[i].image_url, &station_image) != ESP_OK)
            {
                ESP_LOGE(TAG, "Cannot download station image: %s", radio_stations[i].title);
                if (station_image.data)
                    free(station_image.data);

                label = lv_label_create(button);
                lv_label_set_text(label, radio_stations[i].title);
                lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
                lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);

                img = lv_img_create(button);
                lv_img_set_src(img, &radio_128x104);
                lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 8);
                continue;
            }

            int h_zoom = (BUTTON_IMAGE_SIZE * LV_IMG_ZOOM_NONE) / station_image.header.w;
            int v_zoom = (BUTTON_IMAGE_SIZE * LV_IMG_ZOOM_NONE) / station_image.header.h;
            uint16_t zoom = h_zoom < v_zoom ? h_zoom : v_zoom;
            int h_size = (station_image.header.w * zoom) / LV_IMG_ZOOM_NONE;
            int v_size = (station_image.header.h * zoom) / LV_IMG_ZOOM_NONE;

            canvas_buffer = (void *)malloc(h_size * v_size * sizeof(uint16_t));
            // ESP_GOTO_ON_FALSE(canvas_buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "Cannot allocate memmory for canvas buffer");
            canvas = lv_canvas_create(button);
            lv_canvas_set_buffer(canvas, canvas_buffer, h_size, v_size, LV_IMG_CF_TRUE_COLOR);
            lv_canvas_transform(canvas, &station_image, 0, zoom, 0, 0, 0, 0, false);
            lv_obj_center(canvas);

            if (station_image.data)
                free(station_image.data);
        }
    }
    add_refresh_button(radio_stations_size);
    msg_window_hide();
}

static void player_event_cb(player_event_t event, void *subject)
{
    if (event == MP_EVENT_STATE)
    {
        player_state_t *player_state = (player_state_t *)subject;
        ESP_LOGI(TAG, "event = %d, player_state = %d", event, *player_state);
        if (*player_state == MP_STATE_PLAYING || *player_state == MP_STATE_ERROR)
        {
            ESP_LOGI(TAG, "Hide loading window [ Connecting... ]");
            msg_window_hide();
        }
    }
    else if (event == MP_EVENT_SOURCE)
    {
        media_sourece_t *source = (media_sourece_t *)subject;
        if (source->type != MP_SOURCE_TYPE_TUNE_IN)
        {
            if (active_station_button != NULL)
                lv_obj_clear_state(active_station_button, LV_STATE_CHECKED);
            active_station_button = NULL;
            if (active_station_guide_id != NULL)
                free(active_station_guide_id);
            active_station_guide_id = NULL;
        }
    }
}

lv_obj_t *tunein_browser_create(lv_obj_t *parent)
{
    station_list = lv_obj_create(parent);
    lv_obj_set_style_pad_all(station_list, UI_PADDING_ALL, LV_PART_MAIN);
    add_refresh_button(0);
    player_add_event_listener(player_event_cb);
    return station_list;
}