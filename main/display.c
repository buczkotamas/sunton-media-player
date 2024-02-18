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
#include "tunein_browser.h"
#include "dlna.h"
#include "sd_card_browser.h"
#include "msg_window.h"
#include "config_panel.h"
#include "user_config.h"
#include "gui.h"

static const char *TAG = "DISPLAY";

static esp_audio_handle_t player = NULL;

static lcd_backlight_state_t lcd_backlight_state = LCD_BACKLIGHT_FULL;
static lv_timer_t *lcd_backlight_timer = NULL;

static lv_timer_t *audio_time_increment_timer = NULL;
static int32_t audio_time = 0;

static lv_obj_t *home_tab;

static char *album_art_url = NULL;
static lv_obj_t *canvas;
static void *canvas_buffer;

static lv_obj_t *ctrl_btn_mtrx;
static lv_obj_t *vol_btn_mtrx;
static lv_obj_t *audio_time_slider;
static lv_obj_t *audio_time_label;
static lv_obj_t *audio_duration_label;
static lv_obj_t *mute_button;
static lv_obj_t *audio_title_label;
static lv_obj_t *audio_album_label;
static lv_obj_t *audio_artist_label;
static lv_obj_t *media_source_image;

static const char *CTRL_BTN_MAP_PLAY[] = {LV_SYMBOL_PREV, LV_SYMBOL_STOP, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT, NULL};
static const char *CTRL_BTN_MAP_PAUSE[] = {LV_SYMBOL_PREV, LV_SYMBOL_STOP, LV_SYMBOL_PAUSE, LV_SYMBOL_NEXT, NULL};
static const char *CTRL_BTN_MAP_WARNING[] = {LV_SYMBOL_PREV, LV_SYMBOL_STOP, LV_SYMBOL_WARNING, LV_SYMBOL_NEXT, NULL};

LV_IMG_DECLARE(dlna_140x40);
LV_IMG_DECLARE(tunein_140x40);
LV_IMG_DECLARE(microsd_140x40);

static void slider_event_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    int value = (int)lv_slider_get_value(target);
    if (target == audio_time_slider)
    {
        player_seek(value);
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
    }
    else if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id;
        if (target == vol_btn_mtrx)
        {
            id = lv_btnmatrix_get_selected_btn(target);
            int volume = 0;
            player_volume_get(&volume);
            volume += id == 0 ? -1 : 1;
            if (volume > 100)
                volume = 100;
            if (volume < 0)
                volume = 0;
            player_volume_set(volume);

            msg_window_show_volume(volume);
        }
        if (target == ctrl_btn_mtrx)
        {
            id = lv_btnmatrix_get_selected_btn(target);
            if (id == 0 && player_source_get()->type == MP_SOURCE_TYPE_SD_CARD)
                sd_card_browser_prev();
            if (id == 1)
                player_stop();
            if (id == 2)
            {
                if (player_state_get() == MP_STATE_PLAYING)
                    player_pause();
                else
                    player_play();
            }
            if (id == 3 && player_source_get()->type == MP_SOURCE_TYPE_SD_CARD)
                sd_card_browser_next();
        }
    }
}

static esp_err_t show_album_art(char *url)
{
    // if not changed or still invalid, do nothing
    ESP_RETURN_ON_FALSE(url != NULL || album_art_url != NULL, ESP_OK, TAG, "Album art image URL same as before");
    if (url != NULL && album_art_url != NULL)
    {
        ESP_RETURN_ON_FALSE(strcmp(album_art_url, url) != 0, ESP_OK, TAG, "Album art image URL same as before");
        ESP_RETURN_ON_FALSE(strlen(url) > 7 || strlen(url) > 7, ESP_ERR_INVALID_ARG, TAG, "Album art image URL invalid as before");
    }
    //
    if (album_art_url != NULL)
    {
        free(album_art_url);
        album_art_url = NULL;
    }
    if (url != NULL)
    {
        album_art_url = strdup(url);
    }
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
    ESP_RETURN_ON_FALSE(url != NULL && strlen(url) > 7, ESP_ERR_INVALID_ARG, TAG, "Invalid or NULL image URL");

    lv_img_dsc_t cover_art_image = {
        .data = NULL,
    };
    ret = download_image(url, &cover_art_image);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Cannot download image");

    int h_zoom = (UI_MEDIA_ALBUM_ART_WIDTH * LV_IMG_ZOOM_NONE) / cover_art_image.header.w;
    int v_zoom = (UI_MEDIA_ALBUM_ART_HIGHT * LV_IMG_ZOOM_NONE) / cover_art_image.header.h;
    uint16_t zoom = h_zoom < v_zoom ? h_zoom : v_zoom;
    int h_size = (cover_art_image.header.w * zoom) / LV_IMG_ZOOM_NONE;
    int v_size = (cover_art_image.header.h * zoom) / LV_IMG_ZOOM_NONE;

    canvas_buffer = (void *)malloc(h_size * v_size * sizeof(uint16_t));
    ESP_GOTO_ON_FALSE(canvas_buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "Cannot allocate memmory for canvas buffer");

    canvas = lv_canvas_create(home_tab);
    lv_canvas_set_buffer(canvas, canvas_buffer, h_size, v_size, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_transform(canvas, &cover_art_image, 0, zoom, 0, 0, 0, 0, false);
    lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 16);
err:
    if (cover_art_image.data)
        free(cover_art_image.data);
    return ret;
}

static void mute_set(bool mute)
{
    lv_obj_set_style_bg_img_src(mute_button, mute ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX, 0);
}

static void set_audio_time(int32_t time)
{
    audio_time = time;
    lv_slider_set_value(audio_time_slider, time, LV_ANIM_ON);
    lv_label_set_text_fmt(audio_time_label, "%02ld:%02ld:%02ld", time / 3600, (time % 3600) / 60, time % 60);
}

static void set_audio_duration(int32_t duration)
{
    ESP_LOGD(TAG, "set_audio_duration = %ld", duration);
    lv_slider_set_range(audio_time_slider, 0, duration < 1 ? 1 : duration);
    lv_label_set_text_fmt(audio_duration_label, "%02ld:%02ld:%02ld", duration / 3600, (duration % 3600) / 60, duration % 60);
}

static void handle_audio_stop()
{
    lv_btnmatrix_set_map(ctrl_btn_mtrx, CTRL_BTN_MAP_PLAY);
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

void lcd_backlight_set_duty(uint32_t target_duty, int max_fade_time_ms)
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
            lcd_backlight_set_duty(config_get_backlight_full(), 1000);
            lcd_backlight_state = LCD_BACKLIGHT_FULL;
        }
        if (lcd_backlight_timer != NULL)
        {
            ESP_LOGD(TAG, "Reset LCD backlight timer");
            lv_timer_reset(lcd_backlight_timer);
        }
    }
}

static void timer_handle(lv_timer_t *timer)
{
    if (timer == audio_time_increment_timer)
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
            lcd_backlight_set_duty(config_get_backlight_dimm(), 3000);
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

static void metadata_cb(metadata_event_t event, void *subject)
{
    switch (event)
    {
    case METADATA_EVENT:
        lv_label_set_text(audio_title_label, metadata_title_get() == NULL ? "..." : metadata_title_get());
        lv_label_set_text(audio_artist_label, metadata_artist_get() == NULL ? "" : metadata_artist_get());
        lv_label_set_text(audio_album_label, metadata_album_get() == NULL ? "" : metadata_album_get());
        set_audio_duration(metadata_duration_get());
        show_album_art(metadata_image_url_get());
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
            lv_btnmatrix_set_map(ctrl_btn_mtrx, CTRL_BTN_MAP_PAUSE);
            int duration = 0;
            player_audio_duration_get(&duration);
            if (duration != 0)
                set_audio_duration(duration);
            else
                set_audio_duration(metadata_duration_get());
            int time = 0;
            player_audio_time_get(&time);
            set_audio_time(time);
            audio_time_increment_timer = lv_timer_create(timer_handle, 1000, NULL);
            break;
        case MP_STATE_PAUSED:
            lv_btnmatrix_set_map(ctrl_btn_mtrx, CTRL_BTN_MAP_PLAY);
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
            lv_btnmatrix_set_map(ctrl_btn_mtrx, CTRL_BTN_MAP_WARNING);
            set_audio_duration(0);
            break;
        }
        break;
    case MP_EVENT_VOLUME:
        // int *volume = (int *)subject;
        // lv_slider_set_value(vol_slider, *volume, LV_ANIM_ON);
        break;
    case MP_EVENT_MUTE:
        bool *mute = (bool *)subject;
        mute_set(*mute);
        break;
    case MP_EVENT_SOURCE:
        media_sourece_t *source = (media_sourece_t *)subject;
        lv_obj_add_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
        switch (source->type)
        {
        case MP_SOURCE_TYPE_DLNA:
            lv_img_set_src(media_source_image, &dlna_140x40);
            lv_obj_clear_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
            break;
        case MP_SOURCE_TYPE_TUNE_IN:
            lv_img_set_src(media_source_image, &tunein_140x40);
            lv_obj_clear_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
            break;
        case MP_SOURCE_TYPE_SD_CARD:
            lv_img_set_src(media_source_image, &microsd_140x40);
            lv_obj_clear_flag(media_source_image, LV_OBJ_FLAG_HIDDEN);
        default:
            break;
        }
        lv_label_set_text(audio_title_label, "...");
        lv_label_set_text(audio_album_label, "");
        lv_label_set_text(audio_artist_label, "");
        set_audio_duration(0);
        char *album_art_url = NULL;
        show_album_art(album_art_url);
        break;
    default:
        break;
    }
}

void display_lvgl_start(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_pad_all(scr, UI_PADDING_ALL, LV_PART_MAIN);

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_text_font(&style_btn, UI_FONT_XL);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_opa(&style_btn, LV_OPA_50);
    lv_style_set_border_color(&style_btn, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_side(&style_btn, LV_BORDER_SIDE_INTERNAL);

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_clip_corner(&style_bg, true);
    lv_style_set_radius(&style_bg, 10);

    static lv_style_t style_tab;
    lv_style_init(&style_tab);
    lv_style_set_clip_corner(&style_tab, true);
    lv_style_set_pad_all(&style_tab, 0);
    lv_style_set_pad_left(&style_tab, UI_PADDING_ALL);

    lv_obj_t *tabview = lv_tabview_create(scr, LV_DIR_LEFT, 80);

    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_add_style(tab_btns, &style_btn, LV_PART_ITEMS);
    lv_obj_add_style(tab_btns, &style_bg, LV_PART_MAIN);

    /*Add tabs */
    home_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME);
    lv_obj_add_style(home_tab, &style_tab, LV_PART_MAIN);
    lv_obj_t *tunein_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_WIFI);
    lv_obj_add_style(tunein_tab, &style_tab, LV_PART_MAIN);
    lv_obj_t *sdcard_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_SD_CARD);
    lv_obj_add_style(sdcard_tab, &style_tab, LV_PART_MAIN);
    lv_obj_t *settings_tab = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);
    lv_obj_add_style(settings_tab, &style_tab, LV_PART_MAIN);

    lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);

    /*Home tab */
    lv_obj_t *radio_station_list = tunein_browser_create(tunein_tab);
    lv_obj_set_size(radio_station_list, LV_PCT(100), LV_PCT(100));

    lv_obj_t *sd_card_browser = sd_card_browser_create(sdcard_tab, config_get_scan_card_on_boot());
    lv_obj_set_size(sd_card_browser, LV_PCT(100), LV_PCT(100));

    lv_obj_t *config_panel = config_panel_create(settings_tab);
    lv_obj_set_size(config_panel, LV_PCT(100), LV_PCT(100));

    lv_obj_t *footer = lv_obj_create(home_tab);
    lv_obj_set_size(footer, LV_PCT(100), UI_MEDIA_FOOTER_HIGHT);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);

    audio_artist_label = lv_label_create(home_tab);
    lv_obj_set_style_text_font(audio_artist_label, UI_FONT_S, 0);
    lv_label_set_long_mode(audio_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(audio_artist_label, "");
    lv_obj_set_width(audio_artist_label, LV_PCT(100));
    lv_obj_set_style_text_align(audio_artist_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(audio_artist_label, footer, LV_ALIGN_OUT_TOP_MID, 0, -6);

    audio_album_label = lv_label_create(home_tab);
    lv_obj_set_style_text_font(audio_album_label, UI_FONT_S, 0);
    lv_label_set_long_mode(audio_album_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(audio_album_label, "");
    lv_obj_set_width(audio_album_label, LV_PCT(100));
    lv_obj_set_style_text_align(audio_album_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(audio_album_label, audio_artist_label, LV_ALIGN_OUT_TOP_MID, 0, -6);

    audio_title_label = lv_label_create(home_tab);
    lv_obj_set_style_text_font(audio_title_label, UI_FONT_S, 0);
    lv_label_set_long_mode(audio_title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(audio_title_label, "No media");
    lv_obj_set_width(audio_title_label, LV_PCT(100));
    lv_obj_set_style_text_align(audio_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(audio_title_label, audio_album_label, LV_ALIGN_OUT_TOP_MID, 0, -6);

    // mute_button = lv_btn_create(home_tab);
    // lv_obj_add_event_cb(mute_button, button_event_handler, LV_EVENT_CLICKED, NULL);
    // lv_obj_set_size(mute_button, 32, 32);
    // lv_obj_set_style_radius(mute_button, LV_RADIUS_CIRCLE, 0);
    // lv_obj_set_style_bg_img_src(mute_button, LV_SYMBOL_VOLUME_MAX, 0);
    // lv_obj_align_to(mute_button, vol_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);

    // ---------------------------------------------------
    // FOOTER --------------------------------------------
    // ---------------------------------------------------

    audio_time_slider = lv_slider_create(footer);
    lv_obj_set_width(audio_time_slider, 460);
    lv_obj_set_height(audio_time_slider, 6);
    lv_slider_set_range(audio_time_slider, 0, 1);
    lv_slider_set_value(audio_time_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(audio_time_slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(audio_time_slider, LV_ALIGN_TOP_MID, 0, 0);

    audio_time_label = lv_label_create(footer);
    lv_obj_set_style_text_font(audio_time_label, UI_FONT_S, 0);
    lv_label_set_long_mode(audio_time_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(audio_time_label, "00:00:00");
    lv_obj_set_width(audio_time_label, 120);
    lv_obj_set_style_text_align(audio_time_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(audio_time_label, LV_ALIGN_TOP_LEFT, 0, -5);

    audio_duration_label = lv_label_create(footer);
    lv_obj_set_style_text_font(audio_duration_label, UI_FONT_S, 0);
    lv_label_set_long_mode(audio_duration_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(audio_duration_label, "00:00:00");
    lv_obj_set_width(audio_duration_label, 120);
    lv_obj_set_style_text_align(audio_duration_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(audio_duration_label, LV_ALIGN_TOP_RIGHT, 0, -5);

    /* volume button matrix */
    static const char *map[] = {LV_SYMBOL_VOLUME_MID, LV_SYMBOL_VOLUME_MAX, NULL};
    vol_btn_mtrx = lv_btnmatrix_create(footer);
    lv_btnmatrix_set_map(vol_btn_mtrx, map);
    lv_obj_add_style(vol_btn_mtrx, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(vol_btn_mtrx, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_add_event_cb(vol_btn_mtrx, button_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(vol_btn_mtrx, 48 * 3, 48);
    lv_obj_align(vol_btn_mtrx, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* control button matrix */
    ctrl_btn_mtrx = lv_btnmatrix_create(footer);
    lv_btnmatrix_set_map(ctrl_btn_mtrx, CTRL_BTN_MAP_PLAY);
    lv_obj_add_style(ctrl_btn_mtrx, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(ctrl_btn_mtrx, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_add_event_cb(ctrl_btn_mtrx, button_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(ctrl_btn_mtrx, 48 * 6, 48);
    lv_obj_align(ctrl_btn_mtrx, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* media source icon */
    media_source_image = lv_img_create(footer);
    lv_obj_align(media_source_image, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(media_source_image, 140, 40);

    lvgl_port_unlock();

    metadata_add_event_listener(metadata_cb);
    player_add_event_listener(player_event_cb);
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
    lcd_backlight_set_duty(config_get_backlight_full(), 1000);
    uint32_t period = config_get_backlight_timer();
    ESP_LOGD(TAG, "Backlight timer period = %ld", period);
    if (period != 0)
        lcd_backlight_timer = lv_timer_create(timer_handle, config_get_backlight_timer(), NULL);
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