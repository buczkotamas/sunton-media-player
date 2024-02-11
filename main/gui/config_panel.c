#include "config_panel.h"
#include "user_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

static lv_obj_t *conf_panel = NULL;
static lv_obj_t *i2s_dropdown = NULL;
static lv_obj_t *btn_save_and_restart = NULL;
static lv_obj_t *vol_slider = NULL;

static const char *TAG = "CONFIG_PANEL";

static void save_config(void)
{
    uint8_t i2s_output = lv_dropdown_get_selected(i2s_dropdown);
    config_set_i2s_output(i2s_output);
}

static void load_config(void)
{
    uint8_t i2s_output = config_get_i2s_output();
    lv_dropdown_set_selected(i2s_dropdown, i2s_output);
}

static void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (obj == btn_save_and_restart && code == LV_EVENT_CLICKED)
    {
        save_config();
        esp_restart();
    }
}

lv_obj_t *config_panel_create(lv_obj_t *parent)
{
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
    lv_obj_align_to(ssid_label, i2s_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 40);

    lv_obj_t *ssid_txt = lv_textarea_create(conf_panel);
    lv_textarea_set_one_line(ssid_txt, true);
    lv_obj_set_width(ssid_txt, 256);
    lv_obj_align_to(ssid_txt, ssid_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    //
    lv_obj_t *pass_label = lv_label_create(conf_panel);
    lv_obj_add_style(pass_label, &label_style, LV_PART_MAIN);
    lv_label_set_text_static(pass_label, "WiFi Password");
    lv_obj_align_to(pass_label, ssid_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 40);

    lv_obj_t *pass_txt = lv_textarea_create(conf_panel);
    lv_textarea_set_one_line(pass_txt, true);
    lv_obj_set_width(pass_txt, 256);
    lv_obj_align_to(pass_txt, pass_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    //
    // vol_slider = lv_slider_create(conf_panel);
    // lv_slider_set_range(vol_slider, 0, 100);
    // lv_obj_set_size(vol_slider, 180, 6);
    // lv_obj_align(vol_slider, LV_ALIGN_BOTTOM_MID, 0, -8);

    btn_save_and_restart = lv_btn_create(conf_panel);
    lv_obj_add_event_cb(btn_save_and_restart, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn_save_and_restart, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_t *btn_label = lv_label_create(btn_save_and_restart);
    lv_label_set_text(btn_label, "Save and Restart");
    lv_obj_center(btn_label);

    load_config();

    return conf_panel;
}
