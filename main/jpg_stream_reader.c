#include "jpg_stream_reader.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "JPG_STREAM_READER";
static char *stream_url;
static char header_buffer[HEADER_BUFFER_SIZE] = {0};

static TaskHandle_t th_download = NULL;

static esp_http_client_handle_t http_client = NULL;
static jpg_stream_on_frame_cb on_frame_callback;

static bool run_jpg_stream = false;

esp_err_t close_http_client(void)
{
    if (http_client != NULL)
    {
        ESP_RETURN_ON_ERROR(esp_http_client_close(http_client), TAG, "Cannot close HTTP client");
        ESP_RETURN_ON_ERROR(esp_http_client_cleanup(http_client), TAG, "Cannot cleanup HTTP client");
    }
    http_client = NULL;
    return ESP_OK;
}

esp_err_t reconnect_http_client(void)
{
    ESP_RETURN_ON_ERROR(close_http_client(), TAG, "Cannot set HTTP client GET method");
    esp_http_client_config_t config = {
        .url = stream_url,
    };
    http_client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(http_client != NULL, ESP_FAIL, TAG, "Cannot init HTTP client");
    ESP_RETURN_ON_ERROR(esp_http_client_set_method(http_client, HTTP_METHOD_GET), TAG, "Cannot set HTTP client GET method");
    ESP_RETURN_ON_ERROR(esp_http_client_open(http_client, 0), TAG, "Cannot open HTTP client");
    ESP_RETURN_ON_FALSE(esp_http_client_fetch_headers(http_client) >= 0, ESP_FAIL, TAG, "Cannot fetch_headers");
    return ESP_OK;
}

static void decode_and_display(uint8_t *data, uint32_t size)
{
    esp_jpeg_image_output_t img;
    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = data,
        .indata_size = size,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 0,
        },
    };

    esp_err_t dec_ret = esp_jpeg_decode(&jpeg_cfg, &img);

    if (dec_ret == ESP_OK)
        (on_frame_callback)((uint16_t *)jpeg_cfg.outbuf, img);

    if (jpeg_cfg.outbuf != NULL)
        free(jpeg_cfg.outbuf);
}

void download_task(void *p)
{
    if (reconnect_http_client() != ESP_OK)
    {
        ESP_LOGE(TAG, "download_task cannot connect HTTP client");
        jpg_stream_close();
    }
    int content_length = 0;
    while (run_jpg_stream)
    {
        esp_http_client_read_response(http_client, header_buffer, sizeof("--123456789000000000000987654321") + 2);
        esp_http_client_read_response(http_client, header_buffer, sizeof("Content-Type: image/jpeg") + 1);
        esp_http_client_read_response(http_client, header_buffer, sizeof("Content-Length: "));
        memset(header_buffer, '\0', sizeof(header_buffer));
        esp_http_client_read_response(http_client, header_buffer, 1);
        content_length = 0;
        char c = header_buffer[0];
        while (c >= '0' && c <= '9')
        {
            content_length = 10 * content_length + (c - '0');
            esp_http_client_read_response(http_client, header_buffer, 1);
            c = header_buffer[0];
        }
        esp_http_client_read_response(http_client, header_buffer, 3); // last \r\n
        //
        if (content_length > 0)
        {
            char *http_buffer = malloc(content_length);
            int read_len = esp_http_client_read_response(http_client, http_buffer, content_length);

            if (read_len != content_length)
                ESP_LOGW(TAG, "(read_len (%d) != content_length (%d)", read_len, content_length);
            else
                decode_and_display((uint8_t *)http_buffer, content_length);

            if (http_buffer != NULL)
                free(http_buffer);
        }
        else
        {
            ESP_LOGW(TAG, "content_length == 0 reconnecting...");
            if (reconnect_http_client() != ESP_OK)
            {
                ESP_LOGE(TAG, "download_task cannot reconnect HTTP client!");
                jpg_stream_close();
            }
        }
    }
    close_http_client();
    vTaskDelete(NULL);
}

esp_err_t jpg_stream_close(void)
{
    ESP_LOGI(TAG, "closing jpg_stream_reader...");
    run_jpg_stream = false;
    return ESP_OK;
}

esp_err_t jpg_stream_open(char *url, jpg_stream_on_frame_cb on_frame_cb)
{
    ESP_RETURN_ON_FALSE(run_jpg_stream == false, ESP_FAIL, TAG, "jpg_stream already running");
    run_jpg_stream = true;
    stream_url = url;
    on_frame_callback = on_frame_cb;
    BaseType_t ret = xTaskCreate(&download_task, "download_task", 1024 * 4, NULL, 20, &th_download);
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "Cannot create task: download_task, error code: %d", ret);
    return ESP_OK;
}