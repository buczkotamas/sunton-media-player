#include "display.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "driver/ledc.h"

#include "http_client.h"
#include "img_download.h"
#include "player.h"
#include "tunein.h"
#include "dlna.h"
#include "sd_card_browser.h"

#define CANVAS_H_RES 560
#define CANVAS_V_RES 280

static const char *TAG = "DISPLAY";

static esp_audio_handle_t player = NULL;

static lcd_backlight_state_t lcd_backlight_state = LCD_BACKLIGHT_FULL;
static lv_timer_t *lcd_backlight_timer = NULL;

static lv_timer_t *wifi_signal_update_timer = NULL;

static lv_timer_t *audio_time_increment_timer = NULL;
static int32_t audio_time = 0;

static const lv_font_t *font_xs = &lv_font_montserrat_16;
static const lv_font_t *font_s = &lv_font_montserrat_18;
static const lv_font_t *font_m = &lv_font_montserrat_20;
static const lv_font_t *font_l = &lv_font_montserrat_22;
static const lv_font_t *font_xl = &lv_font_montserrat_24;

static lv_obj_t *cower_tile;

static lv_obj_t *canvas;
static void *canvas_buffer;

static lv_obj_t *vol_slider;
static lv_obj_t *audio_time_slider;
static lv_obj_t *audio_time_label;
static lv_obj_t *audio_duration_label;
static lv_obj_t *mute_button;
static lv_obj_t *audio_title_label;
static lv_obj_t *audio_album_label;
static lv_obj_t *audio_artist_label;
static lv_obj_t *media_source_image;
static lv_obj_t *prev_button;
static lv_obj_t *play_button;
static lv_obj_t *stop_button;
static lv_obj_t *next_button;
static lv_obj_t *wifi_label;
// static lv_obj_t *audio_creator_label;

LV_IMG_DECLARE(dlna_160x44);
LV_IMG_DECLARE(tunein_160x44);
LV_IMG_DECLARE(microsd_160x44);

static void sd_card_browser_cb(char *url);

static void slider_event_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    int value = (int)lv_slider_get_value(target);
    if (target == audio_time_slider)
    {
        player_seek(value);
    }
    if (target == vol_slider)
    {
        player_volume_set(value);
    }
}

static void button_event_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (target == mute_button)
            player_mute_set(!player_mute_get());
        if (target == prev_button && player_source_get()->type == MP_SOURCE_TYPE_SD_CARD)
            sd_card_browser_prev();
        if (target == play_button)
        {
            if (player_state_get() == MP_STATE_PLAYING)
                player_pause();
            else
                player_play();
        }
        if (target == stop_button)
            player_stop();
        if (target == next_button && player_source_get()->type == MP_SOURCE_TYPE_SD_CARD)
            sd_card_browser_next();
    }
    else if (code == LV_EVENT_VALUE_CHANGED)
    {
    }
}

static esp_err_t show_album_art(char *url)
{
    esp_err_t ret = ESP_OK;
    if (canvas != NULL)
    {
        lv_obj_del(canvas);
        canvas = NULL;
    }
    if (canvas_buffer != NULL)
    {
        free(canvas_buffer);
        canvas_buffer = NULL;
    }
    ESP_RETURN_ON_FALSE(url != NULL && strlen(url) >= 8, ESP_ERR_INVALID_ARG, TAG, "Invalid or NULL image URL");

    lv_img_dsc_t cover_art_image = {
        .data = NULL,
    };
    ret = download_image(url, &cover_art_image);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Cannot download image");

    int h_zoom = (CANVAS_H_RES * LV_IMG_ZOOM_NONE) / cover_art_image.header.w;
    int v_zoom = (CANVAS_V_RES * LV_IMG_ZOOM_NONE) / cover_art_image.header.h;
    uint16_t zoom = h_zoom < v_zoom ? h_zoom : v_zoom;
    int h_size = (cover_art_image.header.w * zoom) / LV_IMG_ZOOM_NONE;
    int v_size = (cover_art_image.header.h * zoom) / LV_IMG_ZOOM_NONE;

    canvas_buffer = (void *)malloc(h_size * v_size * sizeof(uint16_t));
    ESP_GOTO_ON_FALSE(canvas_buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "Cannot allocate memmory for canvas buffer");

    canvas = lv_canvas_create(cower_tile);
    lv_canvas_set_buffer(canvas, canvas_buffer, h_size, v_size, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_transform(canvas, &cover_art_image, 0, zoom, 0, 0, 0, 0, false);
    lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 16);
err:
    if (cover_art_image.data)
        free(cover_art_image.data);
    return ret;
}

static void tunein_browser_cb(lv_event_t *e)
{
    radio_station_t *radio_station = (radio_station_t *)e->user_data;
    if (tunein_stream_url_get(radio_station) == ESP_OK)
    {
        media_sourece_t source = {
            .type = MP_SOURCE_TYPE_TUNE_IN,
            .url = radio_station->stream_url,
        };
        player_source_set(&source);
        show_album_art(radio_station->image_url);
        // lv_label_set_text(audio_title_label, radio_stations[index].subtitle);
        lv_label_set_text(audio_album_label, radio_station->title);
        lv_label_set_text(audio_artist_label, "");
        player_play();
    }
}

static void mute_set(bool mute)
{
    lv_obj_set_style_bg_img_src(mute_button, mute ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX, 0);
    if (mute)
        lv_obj_clear_flag(vol_slider, LV_OBJ_FLAG_CLICKABLE);
    else
        lv_obj_add_flag(vol_slider, LV_OBJ_FLAG_CLICKABLE);
}

static void set_audio_time(int32_t time)
{
    audio_time = time;
    lv_slider_set_value(audio_time_slider, time, LV_ANIM_ON);
    lv_label_set_text_fmt(audio_time_label, "%02ld:%02ld:%02ld", time / 3600, time / 60, time % 60);
}

static void set_audio_duration(int32_t duration)
{
    lv_slider_set_range(audio_time_slider, 0, duration < 1 ? 1 : duration);
    lv_label_set_text_fmt(audio_duration_label, "%02ld:%02ld:%02ld", duration / 3600, duration / 60, duration % 60);
}

static void handle_audio_stop()
{
    lv_obj_set_style_bg_img_src(play_button, LV_SYMBOL_PLAY, 0);
    if (audio_time_increment_timer != NULL)
    {
        lv_timer_del(audio_time_increment_timer);
        audio_time_increment_timer = NULL;
    }
    set_audio_time(0);
}

//-----------------------------------------------------------------------------------------------------------------
// Technical things
//-----------------------------------------------------------------------------------------------------------------

static void lcd_backlight_set_duty(uint32_t target_duty, int max_fade_time_ms)
{
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LCD_BACKLIGHT_CHANNEL, target_duty, max_fade_time_ms);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LCD_BACKLIGHT_CHANNEL, LEDC_FADE_NO_WAIT);
}

void lvgl_touchpad_feedback_cb(lv_indev_drv_t *, uint8_t event)
{
    if (event == LV_EVENT_PRESSED || event == LV_EVENT_RELEASED)
    {
        if (lcd_backlight_state != LCD_BACKLIGHT_FULL)
        {
            lcd_backlight_set_duty(LCD_BACKLIGHT_FULL_DUTY, 1000);
            lcd_backlight_state = LCD_BACKLIGHT_FULL;
        }
        ESP_LOGD(TAG, "Reset LCD backlight timer");
        lv_timer_reset(lcd_backlight_timer);
    }
}

static void timer_handle(lv_timer_t *timer)
{
    if (timer == wifi_signal_update_timer)
    {
        lv_label_set_text_fmt(wifi_label, "%s %d %%", LV_SYMBOL_WIFI, http_client_wifi_signal_quality_get());
    }
    else if (timer == audio_time_increment_timer)
    {
        set_audio_time(++audio_time);
    }
    else if (timer == lcd_backlight_timer)
    {
        switch (lcd_backlight_state)
        {
        case LCD_BACKLIGHT_DIMM:
            ESP_LOGD(TAG, "Change LCD backlight to OFF");
            lcd_backlight_set_duty(LCD_BACKLIGHT_OFF_DUTY, 3000);
            lcd_backlight_state = LCD_BACKLIGHT_OFF;
            break;
        case LCD_BACKLIGHT_FULL:
            ESP_LOGD(TAG, "Change LCD backlight to DIMM");
            lcd_backlight_set_duty(LCD_BACKLIGHT_DIMM_DUTY, 3000);
            lcd_backlight_state = LCD_BACKLIGHT_DIMM;
            break;
        case LCD_BACKLIGHT_OFF:
        default:
            break;
        }
    }
    else
    {
        ESP_LOGW(TAG, "Unknown timer");
    }
}

static void dlna_event_handler(dlna_event_t event, void *subject)
{
    switch (event)
    {
    case DLNA_EVENT_METADATA:
        audio_metadata_t *metadata = (audio_metadata_t *)subject;
        lv_label_set_text(audio_title_label, metadata->title == NULL ? "Unknown track" : metadata->title);
        lv_label_set_text(audio_album_label, metadata->album == NULL ? "Unknown album" : metadata->album);
        lv_label_set_text(audio_artist_label, metadata->artist == NULL ? "Unknown artist" : metadata->artist);
        set_audio_duration(metadata->duration);
        show_album_art(metadata->albumArtURI);
        break;
    default:
        break;
    }
}

static void player_event_cb(player_event_t event, void *subject)
{
    switch (event)
    {
    case MP_EVENT_STATE:
        player_state_t *player_state = (player_state_t *)subject;
        switch (*player_state)
        {
        case MP_STATE_PLAYING:
            lv_obj_set_style_bg_img_src(play_button, LV_SYMBOL_PAUSE, 0);

            int duration = 0;
            player_audio_duration_get(&duration);
            if (duration != 0)
                set_audio_duration(duration);

            int time = 0;
            player_audio_time_get(&time);
            set_audio_time(time);
            audio_time_increment_timer = lv_timer_create(timer_handle, 1000, NULL);
            break;
        case MP_STATE_PAUSED:
            lv_obj_set_style_bg_img_src(play_button, LV_SYMBOL_PLAY, 0);
            if (audio_time_increment_timer != NULL)
            {
                lv_timer_del(audio_time_increment_timer);
                audio_time_increment_timer = NULL;
            }
            break;
        case MP_STATE_NO_MEDIA:
        case MP_STATE_STOPPED:
            handle_audio_stop();
            break;
        case MP_STATE_FINISHED:
            handle_audio_stop();
            if (player_source_get()->type == MP_SOURCE_TYPE_SD_CARD)
                sd_card_browser_next();
            break;
        case MP_STATE_TRANSITIONING:
        case MP_STATE_ERROR:
        default:
            handle_audio_stop();
            lv_obj_set_style_bg_img_src(play_button, LV_SYMBOL_WARNING, 0);
            set_audio_duration(0);
            break;
        }
        break;
    case MP_EVENT_VOLUME:
        int *volume = (int *)subject;
        lv_slider_set_value(vol_slider, *volume, LV_ANIM_ON);
        break;
    case MP_EVENT_MUTE:
        bool *mute = (bool *)subject;
        mute_set(*mute);
        break;
    case MP_EVENT_ICY_METADATA:
        audio_metadata_t *metadata = (audio_metadata_t *)subject;
        lv_label_set_text(audio_title_label, metadata->title == NULL ? "Unknown track" : metadata->title);
        break;
    case MP_EVENT_SOURCE:
        media_sourece_t *source = (media_sourece_t *)subject;
        lv_obj_add_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
        switch (source->type)
        {
        case MP_SOURCE_TYPE_DLNA:
            lv_img_set_src(media_source_image, &dlna_160x44);
            lv_obj_clear_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
            break;
        case MP_SOURCE_TYPE_TUNE_IN:
            lv_img_set_src(media_source_image, &tunein_160x44);
            lv_obj_clear_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
            break;
        case MP_SOURCE_TYPE_SD_CARD:
            lv_img_set_src(media_source_image, &microsd_160x44);
            lv_obj_clear_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
        default:
            break;
        }
        lv_label_set_text(audio_title_label, "Unknown track");
        lv_label_set_text(audio_album_label, "Unknown album");
        lv_label_set_text(audio_artist_label, "Unknown artist");
        set_audio_duration(0);
        char *album_art_url = NULL;
        show_album_art(album_art_url);
        break;
    default:
        break;
    }
}

static void sd_card_browser_cb(char *url)
{
    media_sourece_t source = {
        .type = MP_SOURCE_TYPE_SD_CARD,
        .url = url,
    };
    player_source_set(&source);
    player_play();
    lv_label_set_text(audio_title_label, url);
    lv_label_set_text(audio_album_label, "");
    lv_label_set_text(audio_artist_label, "");
    char *slash = strrchr(url, '/');
    if (slash)
    {
        int folder_length = slash - url;
        char *cover = malloc(folder_length + 10 + 1); // "/cover.jpg" + '/0'
        strncpy(cover, url, folder_length);
        strcpy(cover + folder_length, "/cover.jpg");
        show_album_art(cover);
        free(cover);
    }
}

void display_lvgl_start(void)
{
    lvgl_port_lock(0);

    lv_coord_t footer_height = 120;

    lv_obj_t *scr = lv_scr_act();

    lv_obj_t *main_tileview = lv_tileview_create(scr);
    lv_obj_set_size(main_tileview, LCD_H_RES, LCD_V_RES - footer_height);

    lv_obj_t *local_tile = lv_tileview_add_tile(main_tileview, 0, 0, LV_DIR_RIGHT);
    cower_tile = lv_tileview_add_tile(main_tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    lv_obj_t *radio_tile = lv_tileview_add_tile(main_tileview, 2, 0, LV_DIR_LEFT);
    lv_obj_set_tile(main_tileview, cower_tile, LV_ANIM_OFF);

    lv_obj_t *radio_station_list = tunein_browser_create(radio_tile, tunein_browser_cb);
    lv_obj_set_size(radio_station_list, LV_PCT(100), LV_PCT(100));

    lv_obj_t *file_browser = sd_card_browser_create(local_tile, sd_card_browser_cb);
    lv_obj_set_size(file_browser, LV_PCT(100), LV_PCT(100));

    wifi_label = lv_label_create(cower_tile);
    lv_obj_set_style_text_font(wifi_label, font_s, 0);
    lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_fmt(wifi_label, "%s %d %%", LV_SYMBOL_WIFI, http_client_wifi_signal_quality_get());
    lv_obj_set_width(wifi_label, 100);
    lv_obj_set_style_text_align(wifi_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align_to(wifi_label, cower_tile, LV_ALIGN_TOP_LEFT, 10, 10);

    audio_title_label = lv_label_create(cower_tile);
    lv_obj_set_style_text_font(audio_title_label, font_m, 0);
    lv_label_set_long_mode(audio_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(audio_title_label, "No media");
    lv_obj_set_width(audio_title_label, 700);
    lv_obj_set_style_text_align(audio_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(audio_title_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    vol_slider = lv_slider_create(scr);
    lv_coord_t def_height = lv_obj_get_style_height(vol_slider, LV_PART_MAIN);
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, 16, LV_ANIM_OFF);
    lv_obj_set_size(vol_slider, def_height, CANVAS_V_RES - 24);
    lv_obj_add_event_cb(vol_slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(vol_slider, LV_ALIGN_TOP_RIGHT, -32, 20);

    mute_button = lv_btn_create(scr);
    lv_obj_add_event_cb(mute_button, button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(mute_button, 32, 32);
    lv_obj_set_style_radius(mute_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_img_src(mute_button, LV_SYMBOL_VOLUME_MAX, 0);
    lv_obj_align_to(mute_button, vol_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);

    // ---------------------------------------------------
    // FOOTER --------------------------------------------
    // ---------------------------------------------------
    lv_obj_t *footer = lv_obj_create(scr);
    lv_obj_set_size(footer, LCD_H_RES, footer_height);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);

    audio_time_slider = lv_slider_create(footer);
    lv_obj_set_width(audio_time_slider, LCD_H_RES - 260);
    lv_slider_set_range(audio_time_slider, 0, 1);
    lv_slider_set_value(audio_time_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(audio_time_slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(audio_time_slider, LV_ALIGN_TOP_MID, 0, 0);

    prev_button = lv_btn_create(footer);
    lv_obj_add_event_cb(prev_button, button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(prev_button, 48, 48);
    lv_obj_set_style_radius(prev_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_img_src(prev_button, LV_SYMBOL_PREV, 0);
    lv_obj_align(prev_button, LV_ALIGN_BOTTOM_MID, -32 - 64, 0);

    stop_button = lv_btn_create(footer);
    lv_obj_add_event_cb(stop_button, button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(stop_button, 48, 48);
    lv_obj_set_style_radius(stop_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_img_src(stop_button, LV_SYMBOL_STOP, 0);
    lv_obj_align(stop_button, LV_ALIGN_BOTTOM_MID, -32, 0);

    play_button = lv_btn_create(footer);
    lv_obj_add_event_cb(play_button, button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(play_button, 48, 48);
    lv_obj_set_style_radius(play_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_img_src(play_button, LV_SYMBOL_PLAY, 0);
    lv_obj_align(play_button, LV_ALIGN_BOTTOM_MID, 32, 0);

    next_button = lv_btn_create(footer);
    lv_obj_add_event_cb(next_button, button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_set_size(next_button, 48, 48);
    lv_obj_set_style_radius(next_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_img_src(next_button, LV_SYMBOL_NEXT, 0);
    lv_obj_align(next_button, LV_ALIGN_BOTTOM_MID, 32 + 64, 0);

    audio_time_label = lv_label_create(footer);
    lv_obj_set_style_text_font(audio_time_label, font_s, 0);
    lv_label_set_long_mode(audio_time_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(audio_time_label, "00:00:00");
    lv_obj_set_width(audio_time_label, (LCD_H_RES - 240) / 2);
    lv_obj_set_style_text_align(audio_time_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(audio_time_label, LV_ALIGN_TOP_LEFT, 0, -5);

    audio_duration_label = lv_label_create(footer);
    lv_obj_set_style_text_font(audio_duration_label, font_s, 0);
    lv_label_set_long_mode(audio_duration_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(audio_duration_label, "00:00:00");
    lv_obj_set_width(audio_duration_label, (LCD_H_RES - 240) / 2);
    lv_obj_set_style_text_align(audio_duration_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(audio_duration_label, LV_ALIGN_TOP_RIGHT, 0, -5);

    audio_album_label = lv_label_create(footer);
    lv_obj_set_style_text_font(audio_album_label, font_s, 0);
    lv_label_set_long_mode(audio_album_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(audio_album_label, "");
    lv_obj_set_width(audio_album_label, LCD_H_RES / 2 - 64);
    lv_obj_set_style_text_align(audio_album_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(audio_album_label, LV_ALIGN_LEFT_MID, 0, 0);

    audio_artist_label = lv_label_create(footer);
    lv_obj_set_style_text_font(audio_artist_label, font_s, 0);
    lv_label_set_long_mode(audio_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(audio_artist_label, "");
    lv_obj_set_width(audio_artist_label, LCD_H_RES / 2 - 64);
    lv_obj_set_style_text_align(audio_artist_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(audio_artist_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    media_source_image = lv_img_create(footer);
    lv_obj_align(media_source_image, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(media_source_image, 160, 44);

    lvgl_port_unlock();

    dlna_add_event_listener(dlna_event_handler);
    player_add_event_listener(player_event_cb);

    wifi_signal_update_timer = lv_timer_create(timer_handle, 10 * 1000, NULL);
}

static void lcd_backlight_init()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LCD_BACKLIGHT_RESOLUTION,
        .freq_hz = LCD_BACKLIGHT_FREQ_HZ,
    };
    ledc_channel_config_t ledc_channel = {
        .channel = LCD_BACKLIGHT_CHANNEL,
        .gpio_num = 2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ledc_timer_config(&ledc_timer);
    ledc_fade_func_install(0);
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    lcd_backlight_set_duty(LCD_BACKLIGHT_FULL_DUTY, 1000);

    lcd_backlight_timer = lv_timer_create(timer_handle, LCD_BACKLIGHT_TIMER_MS, NULL);
}

esp_err_t display_lvgl_init(esp_lcd_handle_t esp_lcd, esp_audio_handle_t audio_handle)
{
    player = audio_handle;

    ESP_LOGI(TAG, "LVGL port - init");
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 4096,       /* LVGL task stack size */
        .task_affinity = 1,       /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "LVGL port - add display");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = esp_lcd.io_handle,
        .panel_handle = esp_lcd.panel_handle,
        .buffer_size = LV_DISPLAY_BUFFER_SIZE,
        .double_buffer = LV_DISPLAY_DOUBLE_BUFFER,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = LCD_SWAP_XY,
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
        },
        .flags = {
            .buff_dma = LV_DISPLAY_BUFFER_DMA,
            .buff_spiram = LV_DISPLAY_BUFFER_SPIRAM,
        }};
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    ESP_LOGI(TAG, "LVGL port - add touch");
    lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = esp_lcd.touch_handle,
        .feedback_cb = lvgl_touchpad_feedback_cb,
    };
    lvgl_port_add_touch(&touch_cfg);

    lcd_backlight_init();

    return ESP_OK;
}