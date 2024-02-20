#include "camera_view.h"
#include "jpg_stream_reader.h"
#include "gui.h"
#include "esp_log.h"

static const char *TAG = "CAMERA_VIEW";
static const char *CAM_BTN_MAP[] = {"Start", "Stop", NULL};

static lv_obj_t *cam_view_panel = NULL;
static lv_obj_t *cam_btn_matrix = NULL;
static lv_obj_t *cam_image = NULL;

static void on_frame_cb(uint16_t *data, esp_jpeg_image_output_t img)
{
    lv_canvas_set_buffer(cam_image, data, img.width, img.height, LV_IMG_CF_TRUE_COLOR);
    lv_obj_invalidate(cam_image);
}

static void start_cam_view()
{
    esp_err_t init = jpg_stream_open("http://192.168.0.51?vf=0&hm=1&fs=5&led=1", on_frame_cb);
    ESP_LOGI(TAG, "Camera view init: %d", init);
}

static void button_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        uint32_t id = lv_btnmatrix_get_selected_btn(target);
        if (id == 0)
            start_cam_view();
        if (id == 1)
            jpg_stream_close();
    }
}

lv_obj_t *camera_view_create(lv_obj_t *parent)
{
    cam_view_panel = lv_obj_create(parent);
    lv_obj_set_style_pad_all(cam_view_panel, UI_PADDING_ALL, LV_PART_MAIN);

    cam_image = lv_canvas_create(cam_view_panel);
    lv_obj_set_style_border_color(cam_image, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_border_width(cam_image, 1, LV_PART_MAIN);
    //lv_obj_set_size(cam_image, 240, 240);
    lv_obj_align(cam_image, LV_ALIGN_TOP_MID, 0, 48);

    cam_btn_matrix = lv_btnmatrix_create(cam_view_panel);
    lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(cam_btn_matrix, UI_FONT_M, LV_PART_MAIN);
    lv_obj_add_event_cb(cam_btn_matrix, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_height(cam_btn_matrix, 48);
    lv_obj_align(cam_btn_matrix, LV_ALIGN_BOTTOM_MID, 0, -48);

    return cam_view_panel;
}
