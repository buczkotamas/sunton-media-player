#include "msg_window.h"

static lv_obj_t *msg_window = NULL;
static lv_obj_t *msg_label = NULL;
static lv_obj_t *msg_bar = NULL;
static lv_obj_t *msg_btn_matrix = NULL;
static lv_timer_t *msg_timer = NULL;

static const char *BTN_MAP_OK[] = {"OK", NULL};
static const char *BTN_MAP_CANCEL[] = {"Cancel", NULL};
static const char *BTN_MAP_OK_CANCEL[] = {"OK", "Cancel", NULL};

static void timer_handler(lv_timer_t *timer)
{
    msg_window_hide();
    lv_timer_del(msg_timer);
    msg_timer = NULL;
}

static void button_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id = lv_btnmatrix_get_selected_btn(target);
    }
    msg_window_hide();
}

static void msg_window_create()
{
    msg_window = lv_obj_create(lv_layer_top());
    lv_obj_set_size(msg_window, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(msg_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(msg_window);

    msg_label = lv_label_create(msg_window);
    lv_obj_set_style_text_font(msg_label, UI_FONT_XL, 0);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 0);

    msg_bar = lv_bar_create(msg_window);
    lv_obj_set_style_min_width(msg_bar, 128, LV_PART_MAIN);
    lv_obj_set_height(msg_bar, 8);
    lv_obj_align_to(msg_bar, msg_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    msg_btn_matrix = lv_btnmatrix_create(msg_window);
    lv_obj_add_style(msg_btn_matrix, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(msg_btn_matrix, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(msg_btn_matrix, UI_FONT_M, LV_PART_MAIN);
    lv_obj_add_event_cb(msg_btn_matrix, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_height(msg_btn_matrix, 48);
    lv_obj_align_to(msg_btn_matrix, msg_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
}

void msg_window_hide(void)
{
    if (msg_window != NULL)
        lv_obj_add_flag(msg_window, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    // lv_refr_now(NULL);
}

static void _msg_window_show(bool modal, int bar, bool ok, bool cancel, int hide_in_ms, const char *fmt, va_list args)
{
    int len = lv_vsnprintf(NULL, 0, fmt, args);
    char *text = (char *)calloc(len + 1, sizeof(char));
    if (text == NULL)
        return;
    lv_vsnprintf(text, len + 1, fmt, args);

    if (msg_window == NULL)
        msg_window_create();
    lv_label_set_text(msg_label, text);
    free(text);

    lv_obj_clear_flag(msg_btn_matrix, LV_OBJ_FLAG_HIDDEN);
    if (ok && cancel)
    {
        lv_btnmatrix_set_map(msg_btn_matrix, BTN_MAP_OK_CANCEL);
        lv_obj_set_width(msg_btn_matrix, 5 * 48);
    }
    else if (ok)
    {
        lv_btnmatrix_set_map(msg_btn_matrix, BTN_MAP_OK);
        lv_obj_set_width(msg_btn_matrix, 3 * 48);
    }
    else if (cancel)
    {
        lv_btnmatrix_set_map(msg_btn_matrix, BTN_MAP_CANCEL);
        lv_obj_set_width(msg_btn_matrix, 3 * 48);
    }
    else
    {
        lv_obj_add_flag(msg_btn_matrix, LV_OBJ_FLAG_HIDDEN);
    }

    if (bar >= 0)
    {
        lv_obj_clear_flag(msg_bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(msg_bar, bar, LV_ANIM_ON);
        lv_obj_align_to(msg_btn_matrix, msg_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
    }
    else
    {
        lv_obj_add_flag(msg_bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(msg_bar, 0, LV_ANIM_OFF);
        lv_obj_align_to(msg_btn_matrix, msg_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
    }

    if (modal)
        lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(msg_window, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);

    if (hide_in_ms > 0)
    {
        if (msg_timer == NULL)
        {
            msg_timer = lv_timer_create(timer_handler, hide_in_ms, NULL);
        }
        else
        {
            lv_timer_set_period(msg_timer, hide_in_ms);
            lv_timer_reset(msg_timer);
        }
    }
    else
    {
        if (msg_timer != NULL)
            lv_timer_del(msg_timer);
        msg_timer = NULL;
    }
}

void msg_window_show(bool modal, int bar, bool ok, bool cancel, int hide_in_ms, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _msg_window_show(modal, bar, ok, cancel, hide_in_ms, fmt, args);
    va_end(args);
}

void msg_window_show_volume(int volume)
{
    msg_window_show(false, volume, false, false, 2000, "%s Volume: %d", LV_SYMBOL_VOLUME_MAX, volume);
    lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
}

void msg_window_show_text(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _msg_window_show(true, -1, false, false, 0, fmt, args);
    va_end(args);
}

void msg_window_show_ok(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _msg_window_show(true, -1, true, false, 0, fmt, args);
    va_end(args);
}
