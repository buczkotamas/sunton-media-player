#include "config_panel.h"
#include "user_config.h"
#include "http_client.h"

#include "display.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

static lv_obj_t *conf_panel = NULL;
static lv_obj_t *i2s_dropdown = NULL;
static lv_obj_t *btn_save_and_restart = NULL;
static lv_obj_t *ssid_txt = NULL;
static lv_obj_t *pass_txt = NULL;
static lv_obj_t *keyboard = NULL;
static lv_obj_t *backlight_full_slider = NULL;
static lv_obj_t *backlight_dimm_slider = NULL;
static lv_obj_t *backlight_timer_dropdown = NULL;
static lv_obj_t *scan_sd_card_switch = NULL;
static lv_obj_t *wifi_label = NULL;

static lv_timer_t *wifi_signal_update_timer = NULL;

static const char *TAG = "CONFIG_PANEL";

static void timer_handle(lv_timer_t *timer)
{
    if (timer == wifi_signal_update_timer)
    {
        lv_label_set_text_fmt(wifi_label, "%s Wifi Signal: %d %%", LV_SYMBOL_WIFI, http_client_wifi_signal_quality_get());
    }
}

static void save_config(void)
{
    config_set_i2s_output(lv_dropdown_get_selected(i2s_dropdown));
    config_set_backlight_full(lv_slider_get_value(backlight_full_slider));
    config_set_backlight_dimm(lv_slider_get_value(backlight_dimm_slider));
    uint16_t bl_timer = lv_dropdown_get_selected(backlight_timer_dropdown);
    // "Off\n1 minute\n3 minutes\n5 minutes\n15 minutes\n30 minutes\n1 hour"
    if (bl_timer == 0)
        config_set_backlight_timer(0);
    else if (bl_timer == 1)
        config_set_backlight_timer(1000 * 60);
    else if (bl_timer == 2)
        config_set_backlight_timer(1000 * 60 * 5);
    else if (bl_timer == 3)
        config_set_backlight_timer(1000 * 60 * 15);
    else if (bl_timer == 4)
        config_set_backlight_timer(1000 * 60 * 30);
    else if (bl_timer == 5)
        config_set_backlight_timer(1000 * 60 * 60);

    config_set_scan_card_on_boot(lv_obj_has_state(scan_sd_card_switch, LV_STATE_CHECKED));
}

static void load_config(void)
{
    lv_dropdown_set_selected(i2s_dropdown, config_get_i2s_output());
    lv_slider_set_value(backlight_full_slider, config_get_backlight_full(), LV_ANIM_OFF);
    lv_slider_set_value(backlight_dimm_slider, config_get_backlight_dimm(), LV_ANIM_OFF);

    if (config_get_scan_card_on_boot())
        lv_obj_add_state(scan_sd_card_switch, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(scan_sd_card_switch, LV_STATE_CHECKED);

    // "Off\n1 minute\n3 minutes\n5 minutes\n15 minutes\n30 minutes\n1 hour"
    uint32_t timeout = config_get_backlight_timer();
    if (timeout == 0)
        lv_dropdown_set_selected(backlight_timer_dropdown, 0);
    else if (timeout == 1000 * 60)
        lv_dropdown_set_selected(backlight_timer_dropdown, 1);
    else if (timeout == 1000 * 60 * 5)
        lv_dropdown_set_selected(backlight_timer_dropdown, 2);
    else if (timeout == 1000 * 60 * 15)
        lv_dropdown_set_selected(backlight_timer_dropdown, 3);
    else if (timeout == 1000 * 60 * 30)
        lv_dropdown_set_selected(backlight_timer_dropdown, 4);
    else if (timeout == 1000 * 60 * 60)
        lv_dropdown_set_selected(backlight_timer_dropdown, 5);
}

static void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    if (target == btn_save_and_restart && code == LV_EVENT_CLICKED)
    {
        save_config();
        esp_restart();
    }
    if (target == pass_txt || target == ssid_txt)
    {
        if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED)
        {
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(keyboard, target);
        }
        if (code == LV_EVENT_DEFOCUSED)
        {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (target == backlight_full_slider || target == backlight_dimm_slider)
    {
        int value = (int)lv_slider_get_value(target);
        lcd_backlight_set_duty(value, 100);
    }
}

lv_obj_t *config_panel_create(lv_obj_t *parent)
{
    int row_height = 36;
    conf_panel = lv_obj_create(parent);
    lv_obj_center(conf_panel);

    static lv_style_t label_style;
    lv_style_init(&label_style);
    lv_style_set_text_font(&label_style, UI_FONT_S);
    lv_style_set_text_align(&label_style, LV_TEXT_ALIGN_LEFT);
    lv_style_set_width(&label_style, 200);
    //
    lv_obj_t *i2s_label = lv_label_create(conf_panel);
    lv_obj_add_style(i2s_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(i2s_label, "I2S Audio Output");
    lv_obj_align(i2s_label, LV_ALIGN_TOP_LEFT, 0, 0);

    i2s_dropdown = lv_dropdown_create(conf_panel);
    lv_dropdown_set_options(i2s_dropdown, "Built In\nUART");
    lv_obj_align_to(i2s_dropdown, i2s_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *ssid_label = lv_label_create(conf_panel);
    lv_obj_add_style(ssid_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(ssid_label, "WiFi SSID");
    lv_obj_align_to(ssid_label, i2s_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);

    ssid_txt = lv_textarea_create(conf_panel);
    lv_obj_add_event_cb(ssid_txt, event_handler, LV_EVENT_ALL, NULL);
    lv_textarea_set_one_line(ssid_txt, true);
    lv_obj_set_width(ssid_txt, 256);
    lv_obj_align_to(ssid_txt, ssid_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *pass_label = lv_label_create(conf_panel);
    lv_obj_add_style(pass_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(pass_label, "WiFi Password");
    lv_obj_align_to(pass_label, ssid_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);

    pass_txt = lv_textarea_create(conf_panel);
    lv_obj_add_event_cb(pass_txt, event_handler, LV_EVENT_ALL, NULL);
    lv_textarea_set_one_line(pass_txt, true);
    lv_obj_set_width(pass_txt, 256);
    lv_obj_align_to(pass_txt, pass_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *brightness_label = lv_label_create(conf_panel);
    lv_obj_add_style(brightness_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(brightness_label, "Full Brightness");
    lv_obj_align_to(brightness_label, pass_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);

    backlight_full_slider = lv_slider_create(conf_panel);
    lv_slider_set_range(backlight_full_slider, 10, 255);
    lv_obj_set_size(backlight_full_slider, 256, 6);
    lv_obj_align_to(backlight_full_slider, brightness_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *dimm_label = lv_label_create(conf_panel);
    lv_obj_add_style(dimm_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(dimm_label, "Dimm Brightness");
    lv_obj_align_to(dimm_label, brightness_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);

    backlight_dimm_slider = lv_slider_create(conf_panel);
    lv_slider_set_range(backlight_dimm_slider, 10, 255);
    lv_obj_set_size(backlight_dimm_slider, 256, 6);
    lv_obj_align_to(backlight_dimm_slider, dimm_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *backlight_timer_label = lv_label_create(conf_panel);
    lv_obj_add_style(backlight_timer_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(backlight_timer_label, "Backlight Timer");
    lv_obj_align_to(backlight_timer_label, dimm_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);

    backlight_timer_dropdown = lv_dropdown_create(conf_panel);
    lv_dropdown_set_options(backlight_timer_dropdown, "Off\n1 minute\n5 minutes\n15 minutes\n30 minutes\n1 hour");
    lv_obj_align_to(backlight_timer_dropdown, backlight_timer_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *scan_label = lv_label_create(conf_panel);
    lv_obj_add_style(scan_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(scan_label, "Scan SD on Boot");
    lv_obj_align_to(scan_label, backlight_timer_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);

    scan_sd_card_switch = lv_switch_create(conf_panel);
    lv_obj_align_to(scan_sd_card_switch, scan_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    wifi_label = lv_label_create(conf_panel);
    lv_obj_add_style(wifi_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_fmt(wifi_label, "%s Wifi Signal: %d %%", LV_SYMBOL_WIFI, http_client_wifi_signal_quality_get());
    lv_obj_align_to(wifi_label, scan_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, row_height);
    //
    btn_save_and_restart = lv_btn_create(conf_panel);
    lv_obj_add_event_cb(btn_save_and_restart, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn_save_and_restart, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_t *btn_label = lv_label_create(btn_save_and_restart);
    lv_label_set_text(btn_label, "Save & Restart");
    lv_obj_center(btn_label);
    //
    /*Create a keyboard - on top of all*/
    keyboard = lv_keyboard_create(conf_panel);
    lv_obj_set_size(keyboard, LV_PCT(100), LV_PCT(50));
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

    load_config();

    lv_obj_add_event_cb(backlight_dimm_slider, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(backlight_full_slider, event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    wifi_signal_update_timer = lv_timer_create(timer_handle, 5 * 1000, NULL);

    return conf_panel;
}
