#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"

#include <stdbool.h> // wtf: needed for playlist.h
#include "sdcard_scan.h"
#include "sdcard_list.h"

#include "sd_card_browser.h"
#include "board_def.h"
#include "player.h"

typedef struct sdcard_list
{
    char *save_file_name;            /*!< Name of file to save URLs */
    char *offset_file_name;          /*!< Name of file to save offset */
    FILE *save_file;                 /*!< File to save urls */
    FILE *offset_file;               /*!< File to save offset of urls */
    char *cur_url;                   /*!< Point to current URL */
    uint16_t url_num;                /*!< Number of URLs */
    uint16_t cur_url_id;             /*!< Current url ID */
    uint32_t total_size_save_file;   /*!< Size of file to save URLs */
    uint32_t total_size_offset_file; /*!< Size of file to save offset */
} sdcard_list_t;

#define SDCARD_LIST_URL_MAX_LENGTH (1024 * 2)

static const char *TAG = "SD_CARD_BROWSER";
static lv_obj_t *file_list = NULL;
static lv_obj_t *dir_up_button = NULL;
static const char *root = SD_MOUNT_POINT;
static char *current_path = NULL;

playlist_operator_handle_t sdcard_list_handle = NULL;

static esp_err_t list_directory(char *path);

static void play(char *url)
{
    char *cover = NULL;
    char *slash = strrchr(url, '/');
    if (slash)
    {
        int folder_length = slash - url;
        cover = malloc(folder_length + 10 + 1); // "/cover.jpg" + '/0'
        strncpy(cover, url, folder_length);
        strcpy(cover + folder_length, "/cover.jpg");
    }
    media_sourece_t source = {
        .type = MP_SOURCE_TYPE_SD_CARD,
        .url = url,
    };
    player_source_set(&source);
    player_play();
    int duration = 0;
    player_audio_duration_get(&duration);
    metadata_set(url, "SD Card", "", duration, url, cover);
    if (cover != NULL)
        free(cover);
}

esp_err_t sd_card_browser_next(void)
{
    char *url = NULL;
    ESP_RETURN_ON_ERROR(sdcard_list_next(sdcard_list_handle, 1, &url), TAG, "Cannot get next track from playlist");
    play(url);
    return ESP_OK;
}

esp_err_t sd_card_browser_prev(void)
{
    char *url = NULL;
    ESP_RETURN_ON_ERROR(sdcard_list_prev(sdcard_list_handle, 1, &url), TAG, "Cannot get previous track from playlist");
    play(url);
    return ESP_OK;
}

static void file_button_handler(lv_event_t *e)
{
    char *url;
    sdcard_list_choose(sdcard_list_handle, (int)e->user_data, &url);
    play(url);
}

static void dir_button_handler(lv_event_t *e)
{
    int path_length = 0;
    char *path = NULL;
    lv_obj_t *button = lv_event_get_target(e);
    if (button == dir_up_button)
    {
        char *slash = strrchr(current_path, '/');
        if (slash)
        {
            path_length = slash - current_path;
            path = malloc(path_length + 1);
            strncpy(path, current_path, path_length);
            path[path_length] = '\0';
        }
    }
    else
    {
        char *file_name = lv_list_get_btn_text(file_list, button);
        path_length = strlen(current_path) + strlen(file_name) + 2; // path + "/" + file + "/0"
        path = (char *)malloc(path_length);
        strcpy(path, current_path);
        strcat(path, "/");
        strcat(path, file_name);
    }
    if (path != NULL)
    {
        list_directory(path);
        free(path);
    }
}

static esp_err_t list_directory(char *path)
{
    esp_err_t ret = ESP_OK;
    lv_obj_t *button = NULL;
    uint32_t pos = 0;
    uint16_t size = 0;
    char *sub_dir = NULL;
    char *url = NULL;

    lv_obj_clean(file_list);
    dir_up_button = NULL;
    if (current_path != NULL)
        free(current_path);
    current_path = strdup(path);

    sdcard_list_t *playlist = (sdcard_list_t *)sdcard_list_handle->playlist;
    ESP_GOTO_ON_FALSE(fseek(playlist->save_file, 0, SEEK_SET) == 0, ESP_FAIL, err, TAG, "Cannot seek save_file file");
    ESP_GOTO_ON_FALSE(fseek(playlist->offset_file, 0, SEEK_SET) == 0, ESP_FAIL, err, TAG, "Cannot seek offset_file file");

    url = (char *)malloc(SDCARD_LIST_URL_MAX_LENGTH);
    sub_dir = (char *)malloc(SDCARD_LIST_URL_MAX_LENGTH);
    memset(sub_dir, 0, SDCARD_LIST_URL_MAX_LENGTH);
    for (int i = 0; i < playlist->url_num; i++)
    {
        memset(url, 0, SDCARD_LIST_URL_MAX_LENGTH);
        ESP_GOTO_ON_FALSE(fread(&pos, 1, sizeof(uint32_t), playlist->offset_file) == sizeof(uint32_t), ESP_FAIL, err, TAG, "Cannot read offset_file file");
        ESP_GOTO_ON_FALSE(fread(&size, 1, sizeof(uint16_t), playlist->offset_file) == sizeof(uint16_t), ESP_FAIL, err, TAG, "Cannot read offset_file file");
        ESP_GOTO_ON_FALSE(fseek(playlist->save_file, pos, SEEK_SET) == 0, ESP_FAIL, err, TAG, "Cannot seek save_file file");
        ESP_GOTO_ON_FALSE(fread(url, 1, size, playlist->save_file) == size, ESP_FAIL, err, TAG, "Cannot read save_file file");

        // e.g.: file://sdcard/Loreena McKennitt/The Visit/08 - The Old Ways.mp3
        char *url_path = url + 6; // url + "file:/"
        int cp_len = strlen(current_path);
        char *url_file_name = url_path + cp_len + 1;
        if (strncmp(current_path, url_path, cp_len) == 0 && url_path[cp_len] == '/')
        {
            char *slash = strchr(url_file_name, '/');
            if (slash == NULL)
            {
                // No more '/' in the path so it's a file
                button = lv_list_add_btn(file_list, LV_SYMBOL_AUDIO, url_file_name);
                lv_obj_add_event_cb(button, file_button_handler, LV_EVENT_CLICKED, i);
                lv_group_remove_obj(button);
            }
            else
            {
                // It's a directory
                int sub_dir_len = slash - url_path - cp_len - 1;
                if (strncmp(sub_dir, url_file_name, sub_dir_len) == 0)
                    continue; // already added? not for sure :)
                memset(sub_dir, 0, SDCARD_LIST_URL_MAX_LENGTH);
                strncpy(sub_dir, url_file_name, sub_dir_len);
                void *icon = strchr(slash + 1, '/') == NULL ? LV_SYMBOL_LIST : LV_SYMBOL_DIRECTORY;
                button = lv_list_add_btn(file_list, icon, sub_dir);
                lv_obj_add_event_cb(button, dir_button_handler, LV_EVENT_CLICKED, NULL);
                lv_group_remove_obj(button);
            }
        }
    }
err:
    if (url != NULL)
        free(url);
    if (sub_dir != NULL)
        free(sub_dir);
    // add path
    lv_obj_t *path_button = lv_list_add_btn(file_list, LV_SYMBOL_SD_CARD, path);
    lv_obj_clear_flag(path_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(path_button, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_group_remove_obj(path_button);
    lv_obj_move_to_index(path_button, 0);
    // add [..]
    if (strcmp(path, root))
    {
        dir_up_button = lv_list_add_btn(file_list, LV_SYMBOL_NEW_LINE, "..");
        lv_obj_add_event_cb(dir_up_button, dir_button_handler, LV_EVENT_CLICKED, NULL);
        lv_group_remove_obj(dir_up_button);
        lv_obj_move_to_index(dir_up_button, 1);
    }
    return ret;
}

void sdcard_url_save_cb(void *user_data, char *url)
{
    playlist_operator_handle_t handle = (playlist_operator_handle_t)user_data;
    if (sdcard_list_save(handle, url) != ESP_OK)
        ESP_LOGE(TAG, "Fail to save sdcard url to sdcard playlist");
}

static void scan_sd_card(void)
{
    sdcard_scan(sdcard_url_save_cb, root, 3, (const char *[]){"mp3", "wav", "flac", "aac", "m4a"}, 5, sdcard_list_handle);
    sdcard_list_show(sdcard_list_handle);
    list_directory(root);
}

static void scan_button_handler(lv_event_t *e)
{
    scan_sd_card();
}

lv_obj_t *sd_card_browser_create(lv_obj_t *parent, bool scan)
{
    file_list = lv_list_create(parent);
    sdcard_list_create(&sdcard_list_handle);
    if (!scan)
    {
        lv_obj_t *button = lv_list_add_btn(file_list, LV_SYMBOL_REFRESH, "Scan Memory Card");
        lv_obj_add_event_cb(button, scan_button_handler, LV_EVENT_CLICKED, NULL);
        lv_group_remove_obj(button);
    }
    else
    {
        scan_sd_card();
    }
    return file_list;
}
