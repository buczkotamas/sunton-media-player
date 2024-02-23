#include "camera_view.h"
#include "jpg_stream_reader.h"
#include "gui.h"
#include "msg_window.h"
#include "esp_log.h"

static const char *API_KEY = "6c19af28-9aea-41df-a680-b75743cd50ae";

static const char *TAG = "CAMERA_VIEW";
static const char *PIC_SIZE_ITEMS = "160x120\n176x144\n240x176\n240X240\n320x240\n400x296\n480x320";
static const int PIC_SIZE_HIGHT[] = {0, 120, 144, 176, 240, 240, 296, 320};
static const char *CAM_BTN_MAP_PLAY[] = {LV_SYMBOL_PLAY, LV_SYMBOL_REFRESH, LV_SYMBOL_LOOP, LV_SYMBOL_CHARGE, NULL};
static const char *CAM_BTN_MAP_STOP[] = {LV_SYMBOL_STOP, LV_SYMBOL_REFRESH, LV_SYMBOL_LOOP, LV_SYMBOL_CHARGE, NULL};

static int h_flip = 0;
static int v_flip = 0;
static int pic_size = 5;
static int led = 0;
static char cam_address[16] = {0};

static bool is_stream_open = false;
static lv_obj_t *cam_select_dropdown = NULL;
static lv_obj_t *cam_btn_matrix = NULL;
static lv_obj_t *pic_size_dropdown = NULL;
static lv_obj_t *cam_image = NULL;
static lv_obj_t *info_label = NULL;
static lv_timer_t *fps_timer = NULL;
static int frame_count = 0;

static void timer_handle(lv_timer_t *timer)
{
    if (timer == fps_timer)
    {
        lv_label_set_text_fmt(info_label, "Camera IP: %s FPS: %d", cam_address, frame_count);
        frame_count = 0;
    }
}

static void stream_event_cb(jpg_stream_event_t event, uint16_t *data, esp_jpeg_image_output_t *img)
{
    switch (event)
    {
    case JPG_STREAM_EVENT_FRAME:
        lv_canvas_set_buffer(cam_image, data, img->width, img->height, LV_IMG_CF_TRUE_COLOR);
        lv_obj_invalidate(cam_image);
        frame_count++;
        break;
    case JPG_STREAM_EVENT_OPEN:
        lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_STOP);
        is_stream_open = true;
        frame_count = 0;
        if (fps_timer == NULL)
            fps_timer = lv_timer_create(timer_handle, 1000, NULL);
        msg_window_hide();
        break;
    case JPG_STREAM_EVENT_CLOSE:
        lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_PLAY);
        is_stream_open = false;
        frame_count = 0;
        if (fps_timer != NULL)
            lv_timer_del(fps_timer);
        fps_timer = NULL;
        lv_label_set_text_static(info_label, "No camera connected");
        msg_window_hide();
        break;
    case JPG_STREAM_EVENT_ERROR:
        msg_window_show_ok("%s  %s", LV_SYMBOL_WARNING, (char *)data);
        is_stream_open = false;
        frame_count = 0;
        if (fps_timer != NULL)
            lv_timer_del(fps_timer);
        fps_timer = NULL;
        lv_label_set_text_static(info_label, "No camera connected");
        break;
    case JPG_STREAM_EVENT_STATUS:
        break;
    }
}

static void cam_stream_open()
{
    msg_window_show_text("Connecting...");
    char url[128] = {0};
    lv_dropdown_get_selected_str(cam_select_dropdown, cam_address, 16);
    sprintf(url, "http://%s?vf=%d&hm=%d&fs=%d&led=%d&key=%s", cam_address, v_flip, h_flip, pic_size, led, API_KEY);
    esp_err_t ret = jpg_stream_open(url, stream_event_cb);
    ESP_LOGI(TAG, "cam_stream_open [%s] = %d", url, ret);

    lv_obj_t *parent = lv_obj_get_parent(cam_image);
    lv_coord_t parent_height = lv_obj_get_content_height(parent);
    int y_offset = (parent_height - UI_MEDIA_FOOTER_HIGHT - PIC_SIZE_HIGHT[pic_size]) / 2;
    lv_obj_align(cam_image, LV_ALIGN_TOP_MID, 0, y_offset);
}

static void button_handler(lv_event_t *e)
{
    uint16_t btn_id;
    char *btn_text;
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (target == cam_btn_matrix)
        {
            btn_id = lv_btnmatrix_get_selected_btn(cam_btn_matrix);
            if (btn_id == 0)
            {
                if (is_stream_open)
                    jpg_stream_close();
                else
                    cam_stream_open();
                return;
            }
            if (btn_id == 1)
                h_flip = h_flip == 0 ? 1 : 0;
            if (btn_id == 2)
                v_flip = v_flip == 0 ? 1 : 0;
            if (btn_id == 3)
                led = led == 0 ? 1 : 0;
            if (is_stream_open)
            {
                jpg_stream_close();
                cam_stream_open();
            }
        }
        else if (target == pic_size_dropdown)
        {
            pic_size = lv_dropdown_get_selected(pic_size_dropdown) + 1;
            btn_text = lv_btnmatrix_get_btn_text(cam_btn_matrix, 0);
            if (is_stream_open)
            {
                jpg_stream_close();
                cam_stream_open();
            }
        }
        else if (target == cam_select_dropdown)
        {
            if (is_stream_open)
            {
                jpg_stream_close();
                cam_stream_open();
            }
        }
    }
}

static const char *get_cameras(void)
{
    return "192.168.0.51\n192.168.0.52";
}

void camera_view_create(lv_obj_t *parent)
{
    cam_image = lv_canvas_create(parent);
    lv_obj_align(cam_image, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *footer = lv_obj_create(parent);
    lv_obj_set_size(footer, LV_PCT(100), UI_MEDIA_FOOTER_HIGHT);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);

    info_label = lv_label_create(footer);
    lv_obj_set_style_text_font(info_label, UI_FONT_S, 0);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(info_label, "No camera connected");
    lv_obj_set_width(info_label, LV_PCT(100));
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 0, -5);

    cam_btn_matrix = lv_btnmatrix_create(footer);
    lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_PLAY);
    lv_btnmatrix_set_btn_ctrl(cam_btn_matrix, 1, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(cam_btn_matrix, 2, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(cam_btn_matrix, 3, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(cam_btn_matrix, UI_FONT_M, LV_PART_MAIN);
    lv_obj_add_event_cb(cam_btn_matrix, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(cam_btn_matrix, 280, 48);
    lv_obj_align(cam_btn_matrix, LV_ALIGN_BOTTOM_MID, 10, 0);

    pic_size_dropdown = lv_dropdown_create(footer);
    lv_dropdown_set_options(pic_size_dropdown, PIC_SIZE_ITEMS);
    lv_dropdown_set_selected(pic_size_dropdown, pic_size - 1);
    lv_obj_add_event_cb(pic_size_dropdown, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_style(pic_size_dropdown, gui_style_dropdown(), LV_PART_MAIN);
    lv_obj_set_width(pic_size_dropdown, 160);
    lv_obj_align(pic_size_dropdown, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    cam_select_dropdown = lv_dropdown_create(footer);
    lv_dropdown_set_options(cam_select_dropdown, get_cameras());
    lv_obj_add_event_cb(cam_select_dropdown, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_style(cam_select_dropdown, gui_style_dropdown(), LV_PART_MAIN);
    lv_obj_set_width(cam_select_dropdown, 180);
    lv_obj_align(cam_select_dropdown, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}
