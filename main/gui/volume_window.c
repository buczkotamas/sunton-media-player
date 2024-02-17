#include "volume_window.h"

static lv_obj_t *vol_label = NULL;
static lv_obj_t *vol_window = NULL;
static lv_obj_t *vol_slider = NULL;

static lv_obj_t *load_label = NULL;
static lv_obj_t *load_window = NULL;

static lv_timer_t *vol_window_timer = NULL;

static void timer_handle(lv_timer_t *timer)
{
    lv_obj_add_flag(vol_window, LV_OBJ_FLAG_HIDDEN);
    lv_timer_del(vol_window_timer);
    vol_window_timer = NULL;
}

static void voulme_window_create(void)
{
    vol_window = lv_obj_create(lv_layer_top());
    lv_obj_set_size(vol_window, 256, 100);
    lv_obj_add_flag(vol_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(vol_window);
    //
    vol_label = lv_label_create(vol_window);
    lv_obj_set_style_text_font(vol_label, UI_FONT_XL, 0);
    lv_label_set_long_mode(vol_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(vol_label, 180);
    lv_obj_set_style_text_align(vol_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(vol_label, LV_ALIGN_TOP_MID, 0, 0);
    //
    vol_slider = lv_slider_create(vol_window);
    lv_slider_set_range(vol_slider, 0, 100);
    lv_obj_set_size(vol_slider, 180, 6);
    lv_obj_align(vol_slider, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void load_window_create(void)
{
    load_window = lv_obj_create(lv_layer_top());
    lv_obj_set_size(load_window, 256, 100);
    lv_obj_add_flag(load_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(load_window);
    //
    load_label = lv_label_create(load_window);
    lv_obj_set_style_text_font(load_label, UI_FONT_XL, 0);
    lv_label_set_long_mode(load_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(load_label, LV_PCT(100));
    lv_obj_set_style_text_align(load_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(load_label, LV_ALIGN_CENTER, 0, 0);
}

void volume_window_show(int volume)
{
    if (vol_window == NULL)
        voulme_window_create();
    lv_label_set_text_fmt(vol_label, "%s Volume: %d", LV_SYMBOL_VOLUME_MAX, volume);
    lv_slider_set_value(vol_slider, volume, LV_ANIM_ON);
    lv_obj_clear_flag(vol_window, LV_OBJ_FLAG_HIDDEN);
    if (vol_window_timer == NULL)
    {
        vol_window_timer = lv_timer_create(timer_handle, 2 * 1000, NULL);
    }
    else
    {
        lv_timer_reset(vol_window_timer);
    }
}

void loading_window_show_fmt(const char *fmt, ...)
{
    if (load_window == NULL)
        load_window_create();

    va_list args;
    va_start(args, fmt);
    uint32_t len = lv_vsnprintf(NULL, 0, fmt, args);
    char *text = 0;
    text = lv_mem_alloc(len + 1);
    LV_ASSERT_MALLOC(text);
    if (text == NULL)
    {
        return;
    }
    text[len] = 0; /*Ensure NULL termination*/
    lv_vsnprintf(text, len + 1, fmt, args);
    va_end(args);
    lv_label_set_text(load_label, text);

    lv_obj_clear_flag(load_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    lv_refr_now(NULL);
}

void loading_window_show(const char *msg)
{
    if (load_window == NULL)
        load_window_create();
    lv_label_set_text_static(load_label, msg == NULL ? "Loading..." : msg);
    lv_obj_clear_flag(load_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    lv_refr_now(NULL);
}

void loading_window_hide(void)
{
    if (load_window != NULL)
        lv_obj_add_flag(load_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    lv_refr_now(NULL);
}