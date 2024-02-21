#include "camera_view.h"
#include "jpg_stream_reader.h"
#include "gui.h"
#include "esp_log.h"

static const char *TAG = "CAMERA_VIEW";
static const char *PIC_SIZE_ITEMS = "96X96\n160x120\n176x144\n240x176\n240X240\n320x240\n400x296\n480x320";
static const char *CAM_BTN_MAP[] = {LV_SYMBOL_STOP, LV_SYMBOL_PLAY, LV_SYMBOL_IMAGE, NULL};
static const char *FLIP_BTN_MAP[] = {LV_SYMBOL_REFRESH, LV_SYMBOL_LOOP, LV_SYMBOL_CHARGE, NULL};

static int h_flip = 0;
static int v_flip = 0;
static int pic_size = 5;
static int led = 0;

static lv_obj_t *cam_view_panel = NULL;
static lv_obj_t *cam_btn_matrix = NULL;
static lv_obj_t *flip_btn_matrix = NULL;
static lv_obj_t *pic_size_dropdown = NULL;
static lv_obj_t *cam_image = NULL;

static void stream_event_cb(jpg_stream_event_t event, uint16_t *data, esp_jpeg_image_output_t *img)
{
    if (event == JPG_STREAM_EVENT_FRAME)
    {
        lv_canvas_set_buffer(cam_image, data, img->width, img->height, LV_IMG_CF_TRUE_COLOR);
        lv_obj_invalidate(cam_image);
    }
}

static void start_cam_view()
{
    char url[64] = {0};
    sprintf(url, "http://192.168.0.51?vf=%d&hm=%d&fs=%d&led=%d", v_flip, h_flip, pic_size, led);
    esp_err_t init = jpg_stream_open(url, stream_event_cb);
    ESP_LOGI(TAG, "Camera view init: %d", init);
}

static void button_handler(lv_event_t *e)
{
    uint32_t id;
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (target == cam_btn_matrix && code == LV_EVENT_VALUE_CHANGED)
    {
        id = lv_btnmatrix_get_selected_btn(target);
        if (id == 0)
            jpg_stream_close();
        if (id == 1)
            start_cam_view();
    }
    if (target == flip_btn_matrix && code == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id = lv_btnmatrix_get_selected_btn(target);
        if (id == 0)
            h_flip = h_flip == 0 ? 1 : 0;
        if (id == 1)
            v_flip = v_flip == 0 ? 1 : 0;
        if (id == 2)
            led = led == 0 ? 1 : 0;
        jpg_stream_close();
        start_cam_view();
    }
    if (target == pic_size_dropdown && code == LV_EVENT_VALUE_CHANGED)
    {
        pic_size = lv_dropdown_get_selected(pic_size_dropdown);
        jpg_stream_close();
        start_cam_view();
    }
}

lv_obj_t *camera_view_create(lv_obj_t *parent)
{
    cam_view_panel = lv_obj_create(parent);
    lv_obj_set_style_pad_all(cam_view_panel, UI_PADDING_ALL, LV_PART_MAIN);

    cam_image = lv_canvas_create(cam_view_panel);
    lv_obj_align(cam_image, LV_ALIGN_TOP_MID, 0, 32);

    cam_btn_matrix = lv_btnmatrix_create(cam_view_panel);
    lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(cam_btn_matrix, UI_FONT_M, LV_PART_MAIN);
    lv_obj_add_event_cb(cam_btn_matrix, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(cam_btn_matrix, 220, 48);
    lv_obj_align(cam_btn_matrix, LV_ALIGN_BOTTOM_MID, 0, -32);

    flip_btn_matrix = lv_btnmatrix_create(cam_view_panel);
    lv_btnmatrix_set_map(flip_btn_matrix, FLIP_BTN_MAP);
    lv_btnmatrix_set_btn_ctrl_all(flip_btn_matrix, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_add_style(flip_btn_matrix, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(flip_btn_matrix, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(flip_btn_matrix, UI_FONT_M, LV_PART_MAIN);
    lv_obj_add_event_cb(flip_btn_matrix, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(flip_btn_matrix, 220, 48);
    lv_obj_align(flip_btn_matrix, LV_ALIGN_BOTTOM_LEFT, 0, -32);

    pic_size_dropdown = lv_dropdown_create(cam_view_panel);
    lv_dropdown_set_options(pic_size_dropdown, PIC_SIZE_ITEMS);
    lv_obj_add_event_cb(pic_size_dropdown, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_width(pic_size_dropdown, 168);
    lv_obj_set_style_border_color(pic_size_dropdown, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_border_width(pic_size_dropdown, 1, LV_PART_MAIN);
    lv_obj_align(pic_size_dropdown, LV_ALIGN_BOTTOM_RIGHT, 0, -32);

    return cam_view_panel;
}
