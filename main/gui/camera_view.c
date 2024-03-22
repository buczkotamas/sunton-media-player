#include "camera_view.h"
#include "jpg_stream_reader.h"
#include "gui.h"
#include "msg_window.h"
#include "esp_log.h"

#include "esp_websocket_client.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

// joystick
#define BTN_NONE -1
#define BTN_UP 0
#define BTN_DOWN 1
#define BTN_LEFT 2
#define BTN_RIGHT 3
static int last_joystick_btn = BTN_NONE;
static const char *JOYSTICK_BTN_PRESSED_MAP[] = {"vi 0", "vi 180", "hi 0", "hi 180"};
static const char *JOYSTICK_BTN_RELEASE_MAP[] = {"vi -1", "vi -1", "hi -1", "hi -1"};

static const char *API_KEY = "6c19af28-9aea-41df-a680-b75743cd50ae";

static const char *TAG = "CAMERA_VIEW";
static const char *PIC_SIZE_ITEMS = "160x120\n176x144\n240x176\n240X240\n320x240\n400x296\n480x320";
static const int PIC_SIZE_HIGHT[] = {0, 120, 144, 176, 240, 240, 296, 320};
static const int PIC_SIZE_WIDTH[] = {0, 160, 176, 240, 240, 320, 400, 480};
static const char *CAM_BTN_MAP_PLAY[] = {LV_SYMBOL_PLAY, LV_SYMBOL_REFRESH, "\n", LV_SYMBOL_UP LV_SYMBOL_DOWN, LV_SYMBOL_RIGHT " " LV_SYMBOL_LEFT, NULL};
static const char *CAM_BTN_MAP_STOP[] = {LV_SYMBOL_STOP, LV_SYMBOL_REFRESH, "\n", LV_SYMBOL_UP LV_SYMBOL_DOWN, LV_SYMBOL_RIGHT " " LV_SYMBOL_LEFT, NULL};

static int rotate = 0;
static int h_flip = 0;
static int v_flip = 0;
static int pic_size = 5;
static int led = 0;
static char cam_address[16] = {0};

static bool is_stream_open = false;
static lv_obj_t *h_slider = NULL;
static lv_obj_t *v_slider = NULL;
static lv_obj_t *cam_select_dropdown = NULL;
static lv_obj_t *cam_btn_matrix = NULL;
static lv_obj_t *joystick = NULL;
static lv_obj_t *pic_size_dropdown = NULL;
static lv_obj_t *cam_image = NULL;
static lv_obj_t *ip_label = NULL;
static lv_obj_t *fsp_label = NULL;
static lv_timer_t *fps_timer = NULL;
static int frame_count = 0;

esp_websocket_client_handle_t client = NULL;

LV_IMG_DECLARE(joystick_160x160);

static uint16_t *canvas_buffer = NULL;
static int *rotate_mtrx = NULL;

static void websocket_send(char *msg)
{
    ESP_LOGI(TAG, "websocket_send: %s", msg);
    esp_websocket_client_send_text(client, msg, strlen(msg), portMAX_DELAY);
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        websocket_send("inf");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        if (data->op_code == WS_TRANSPORT_OPCODES_TEXT)
        {
            char msg[8] = {0};
            int pos;
            char id[3];
            strncpy(msg, data->data_ptr, data->data_len);
            sscanf(msg, "%s %d", id, &pos);
            if (strcmp(id, "hp") == 0)
                lv_slider_set_value(h_slider, pos, LV_ANIM_ON);
            if (strcmp(id, "vp") == 0)
                lv_slider_set_value(v_slider, 180 - pos, LV_ANIM_ON);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

static void websocket_close()
{
    ESP_LOGI(TAG, "Closing websocket if open");
    if (client != NULL)
        esp_websocket_client_destroy(client);
    client = NULL;
}

static void websocket_open()
{
    websocket_close();
    char url[32] = {0};
    sprintf(url, "ws://%s/ws", cam_address);
    ESP_LOGI(TAG, "Connecting to websocket URL: %s", url);
    esp_websocket_client_config_t websocket_cfg = {
        .uri = url,
    };
    client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);
}

static void timer_handle(lv_timer_t *timer)
{
    if (timer == fps_timer)
    {
        lv_label_set_text_fmt(fsp_label, "FPS: %d", frame_count);
        frame_count = 0;
    }
}

static void stream_event_cb(jpg_stream_event_t event, uint16_t *data, esp_jpeg_image_output_t *img)
{
    switch (event)
    {
    case JPG_STREAM_EVENT_FRAME:
        if (rotate == 0 || canvas_buffer == NULL || rotate_mtrx == NULL)
        {
            lv_canvas_set_buffer(cam_image, data, img->width, img->height, LV_IMG_CF_TRUE_COLOR);
        }
        else
        {
            for (int i = 0; i < img->height * img->width; i++)
            {
                canvas_buffer[rotate_mtrx[i]] = data[i];
            }
        }
        lv_obj_invalidate(cam_image);

        // int h_zoom = (480 * LV_IMG_ZOOM_NONE) / img->width;
        // int v_zoom = (320 * LV_IMG_ZOOM_NONE) / img->height;
        // uint16_t zoom = h_zoom < v_zoom ? h_zoom : v_zoom;
        // int h_size = (img->width * zoom) / LV_IMG_ZOOM_NONE;
        // int v_size = (img->height * zoom) / LV_IMG_ZOOM_NONE;
        // lv_img_dsc_t image = {
        //     .data = (uint8_t *)data,
        //     .data_size = img->width * img->height * sizeof(uint16_t),
        //     .header = {
        //         .w = img->width,
        //         .h = img->height,
        //         .cf = LV_IMG_CF_TRUE_COLOR,
        //     }};
        // zoom = 256;
        // lv_canvas_transform(cam_image, &image, 900, zoom, 0, 0, img->width / 2, img->height / 2, false);

        frame_count++;
        break;
    case JPG_STREAM_EVENT_OPEN:
        lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_STOP);
        is_stream_open = true;
        frame_count = 0;
        if (fps_timer == NULL)
            fps_timer = lv_timer_create(timer_handle, 1000, NULL);
        lv_label_set_text_fmt(ip_label, "Camera IP: %s", cam_address);
        msg_window_hide();
        break;
    case JPG_STREAM_EVENT_CLOSE:
        lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_PLAY);
        is_stream_open = false;
        frame_count = 0;
        if (fps_timer != NULL)
            lv_timer_del(fps_timer);
        fps_timer = NULL;
        lv_label_set_text_static(ip_label, "No camera connected");
        lv_label_set_text_static(fsp_label, "FSP: -");
        msg_window_hide();
        break;
    case JPG_STREAM_EVENT_ERROR:
        websocket_close();
        lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_PLAY);
        is_stream_open = false;
        frame_count = 0;
        if (fps_timer != NULL)
            lv_timer_del(fps_timer);
        fps_timer = NULL;
        lv_label_set_text_static(ip_label, "No camera connected");
        lv_label_set_text_static(fsp_label, "FSP: -");
        msg_window_show_ok("%s  %s", LV_SYMBOL_WARNING, (char *)data);
        break;
    case JPG_STREAM_EVENT_STATUS:
        break;
    }
}

static void cam_stream_close()
{
    websocket_close();
    jpg_stream_close();
}

static void cam_stream_open()
{
    msg_window_show_text("Connecting...");

    lv_dropdown_get_selected_str(cam_select_dropdown, cam_address, 16);

    if (!esp_websocket_client_is_connected(client))
        websocket_open();

    char url[128] = {0};
    sprintf(url, "http://%s:81?vf=%d&hm=%d&fs=%d&led=%d&key=%s", cam_address, v_flip, h_flip, pic_size, led, API_KEY);
    esp_err_t ret = jpg_stream_open(url, stream_event_cb);
    ESP_LOGI(TAG, "cam_stream_open [%s] = %d", url, ret);

    if (rotate_mtrx != NULL)
    {
        free(rotate_mtrx);
        rotate_mtrx = NULL;
    }
    if (canvas_buffer != NULL)
    {
        free(canvas_buffer);
        canvas_buffer = NULL;
    }
    int y_offset = 0;
    int x_offset = 0;
    int pic_h = PIC_SIZE_HIGHT[pic_size];
    int pic_w = PIC_SIZE_WIDTH[pic_size];
    if (rotate == 0)
    {
        y_offset = (320 - pic_h) / 2;
        x_offset = (480 - pic_w) / 2;
    }
    else
    {
        y_offset = (320 - pic_w) / 2;
        x_offset = (480 - pic_h) / 2;
        rotate_mtrx = (int *)malloc(pic_h * pic_w * sizeof(int));
        if (rotate_mtrx == NULL)
            ESP_LOGE(TAG, "Cannot allocate memmory for rotate_mtrx");
        canvas_buffer = (uint16_t *)malloc(pic_h * pic_w * sizeof(uint16_t));
        if (canvas_buffer == NULL)
            ESP_LOGE(TAG, "Cannot allocate memmory for canvas_buffer");
        lv_canvas_set_buffer(cam_image, canvas_buffer, pic_h, pic_w, LV_IMG_CF_TRUE_COLOR);

        for (int h = 0; h < pic_h; h++)
        {
            for (int w = 0; w < pic_w; w++)
            {
                rotate_mtrx[h * pic_w + w] = (pic_h - (h + 1)) + (pic_h * w);
            }
        }
    }
    lv_obj_align(cam_image, LV_ALIGN_TOP_LEFT, x_offset + 10, y_offset + 10);
}

static void button_handler(lv_event_t *e)
{
    uint16_t btn_id;
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
                    cam_stream_close();
                else
                    cam_stream_open();
                return;
            }
            if (btn_id == 1)
                rotate = rotate == 0 ? 1 : 0;
            if (btn_id == 2)
                v_flip = v_flip == 0 ? 1 : 0;
            if (btn_id == 3)
                h_flip = h_flip == 0 ? 1 : 0;
            if (is_stream_open)
            {
                jpg_stream_close();
                cam_stream_open();
            }
        }
        else if (target == pic_size_dropdown)
        {
            pic_size = lv_dropdown_get_selected(pic_size_dropdown) + 1;
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
                cam_stream_close();
                cam_stream_open();
            }
        }
        else if (target == h_slider && esp_websocket_client_is_connected(client))
        {
            char data[8] = {0};
            sprintf(data, "hc %d", (int)lv_slider_get_value(target));
            websocket_send(data);
        }
        else if (target == v_slider && esp_websocket_client_is_connected(client))
        {
            char data[16] = {0};
            sprintf(data, "vc %d", (int)(180 - lv_slider_get_value(target)));
            websocket_send(data);
        }
    }

    if (target == joystick && esp_websocket_client_is_connected(client))
    {
        lv_point_t p;
        lv_area_t coords;
        lv_obj_get_coords(joystick, &coords);
        lv_indev_get_point(lv_indev_get_act(), &p);
        int x = p.x - ((coords.x1 + coords.x2) / 2);
        int y = p.y - ((coords.y1 + coords.y2) / 2);
        if (code == LV_EVENT_RELEASED || (abs(x) < 16 && abs(y) < 16))
        {
            if (last_joystick_btn != BTN_NONE)
                websocket_send(JOYSTICK_BTN_RELEASE_MAP[last_joystick_btn]);
            last_joystick_btn = BTN_NONE;
        }
        else if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING)
        {
            int button;
            if (x < y)
                button = (x > -y) ? BTN_DOWN : BTN_LEFT;
            else
                button = (x > -y) ? BTN_RIGHT : BTN_UP;
            if (button != last_joystick_btn)
            {
                if (last_joystick_btn != BTN_NONE)
                    websocket_send(JOYSTICK_BTN_RELEASE_MAP[last_joystick_btn]);
                websocket_send(JOYSTICK_BTN_PRESSED_MAP[button]);
            }
            last_joystick_btn = button;
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
    lv_obj_align(cam_image, LV_ALIGN_TOP_LEFT, 10, 10);

    h_slider = lv_slider_create(parent);
    lv_slider_set_range(h_slider, 0, 180);
    lv_slider_set_value(h_slider, 90, LV_ANIM_OFF);
    lv_obj_set_size(h_slider, 440, 6);
    lv_obj_set_style_bg_opa(h_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_event_cb(h_slider, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_pos(h_slider, 30, 350);

    v_slider = lv_slider_create(parent);
    lv_slider_set_range(v_slider, 0, 180);
    lv_slider_set_value(v_slider, 180, LV_ANIM_OFF);
    lv_obj_set_size(v_slider, 6, 280);
    lv_obj_set_style_bg_opa(v_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_event_cb(v_slider, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_pos(v_slider, 510, 30);

    joystick = lv_img_create(parent);
    lv_obj_add_flag(joystick, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(joystick, 160, 160);
    lv_obj_add_event_cb(joystick, button_handler, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(joystick, button_handler, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(joystick, button_handler, LV_EVENT_RELEASED, NULL);
    lv_img_set_src(joystick, &joystick_160x160);
    lv_obj_align(joystick, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    ip_label = lv_label_create(parent);
    lv_obj_set_style_text_font(ip_label, UI_FONT_S, 0);
    lv_label_set_long_mode(ip_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(ip_label, "No camera connected");
    lv_obj_set_width(ip_label, 256);
    lv_obj_set_style_text_align(ip_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align_to(ip_label, h_slider, LV_ALIGN_OUT_BOTTOM_LEFT, -10, 20);

    fsp_label = lv_label_create(parent);
    lv_obj_set_style_text_font(fsp_label, UI_FONT_S, 0);
    lv_label_set_long_mode(fsp_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(fsp_label, "FSP: -");
    lv_obj_set_width(fsp_label, 256);
    lv_obj_set_style_text_align(fsp_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align_to(fsp_label, ip_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);

    cam_select_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(cam_select_dropdown, get_cameras());
    lv_obj_add_event_cb(cam_select_dropdown, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_style(cam_select_dropdown, gui_style_dropdown(), LV_PART_MAIN);
    lv_obj_set_width(cam_select_dropdown, 160);
    lv_obj_align(cam_select_dropdown, LV_ALIGN_TOP_RIGHT, 0, 10);

    cam_btn_matrix = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(cam_btn_matrix, CAM_BTN_MAP_PLAY);
    lv_btnmatrix_set_btn_ctrl(cam_btn_matrix, 1, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(cam_btn_matrix, 2, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_btn_ctrl(cam_btn_matrix, 3, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_main(), LV_PART_MAIN);
    lv_obj_add_style(cam_btn_matrix, gui_style_btnmatrix_items(), LV_PART_ITEMS);
    lv_obj_set_style_radius(cam_btn_matrix, 24, LV_PART_MAIN);
    lv_obj_set_style_text_font(cam_btn_matrix, UI_FONT_M, LV_PART_MAIN);
    lv_obj_add_event_cb(cam_btn_matrix, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_size(cam_btn_matrix, 160, 3 * 48);
    lv_obj_align_to(cam_btn_matrix, cam_select_dropdown, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    pic_size_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(pic_size_dropdown, PIC_SIZE_ITEMS);
    lv_dropdown_set_selected(pic_size_dropdown, pic_size - 1);
    lv_obj_add_event_cb(pic_size_dropdown, button_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_style(pic_size_dropdown, gui_style_dropdown(), LV_PART_MAIN);
    lv_obj_set_width(pic_size_dropdown, 160);
    lv_obj_align_to(pic_size_dropdown, cam_btn_matrix, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}
