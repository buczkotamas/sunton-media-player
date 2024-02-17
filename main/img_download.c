#include "esp_log.h"
#include "esp_check.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_random.h"

#include "board.h"
#include "img_download.h"
#include "jpeg_decoder.h"
#include "http_client.h"
#include "lodepng.h"

#define HTTP_RESPONSE_MAX_SIZE 512 * 1024
static const char *TAG = "IMG_DOWNLOAD";

esp_err_t jpeg_to_rgb565(uint8_t *jpeg_buffer, size_t jpeg_buffer_size, lv_img_dsc_t *lv_img_dsc)
{
    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = jpeg_buffer,
        .indata_size = jpeg_buffer_size,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
#ifdef CONFIG_LV_COLOR_16_SWAP
            .swap_color_bytes = 1,
#else
            .swap_color_bytes = 0,
#endif
        },
    };
    esp_jpeg_image_output_t jpeg_size;
    esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &jpeg_size);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "JPEG decode error");
        return ret;
    }

    ESP_LOGI(TAG, "JPEG decode success, image width = %d height = %d, size = %lu, outbuf = %p",
             jpeg_size.width, jpeg_size.height, jpeg_cfg.outbuf_size, jpeg_cfg.outbuf);
    lv_img_dsc->header.w = jpeg_size.width;
    lv_img_dsc->header.h = jpeg_size.height;
    lv_img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    lv_img_dsc->data_size = jpeg_cfg.outbuf_size;
    lv_img_dsc->data = jpeg_cfg.outbuf;
    return ESP_OK;
}

esp_err_t png_to_rgb565(unsigned char *png_buffer, size_t png_buffer_size, lv_img_dsc_t *lv_img_dsc)
{
    ESP_LOGI(TAG, "lodepng_decode24...");
    unsigned width, height;
    uint8_t *rgb888 = NULL;
    uint16_t *rgb565 = NULL;
    unsigned error = lodepng_decode24(&rgb888, &width, &height, png_buffer, png_buffer_size);
    if (error)
    {
        if (rgb888 != NULL)
        {
            free(rgb888);
        }
        ESP_LOGW(TAG, "PNG (lodepng) decode error: %s", lodepng_error_text(error));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "PNG decode success, image width = %u height = %u", width, height);

    uint32_t out_size = width * height * sizeof(uint16_t);
    rgb565 = (uint16_t *)malloc(out_size);
    if (rgb565 == NULL)
    {
        ESP_LOGE(TAG, "Not enough memmory for PNG->RGB888->RGB565 out buffer");
        return ESP_FAIL;
    }
    // convert 888 to 565
    for (int i = 0; i < width * height; i += 1)
    {
        uint8_t red = rgb888[i * 3];
        uint8_t green = rgb888[i * 3 + 1];
        uint8_t blue = rgb888[i * 3 + 2];
        uint16_t b = (blue >> 3) & 0x1f;
        uint16_t g = ((green >> 2) & 0x3f) << 5;
        uint16_t r = ((red >> 3) & 0x1f) << 11;
        rgb565[i] = (uint16_t)(r | g | b);
    }
    free(rgb888);

    lv_img_dsc->header.w = width;
    lv_img_dsc->header.h = height;
    lv_img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    lv_img_dsc->data_size = out_size;
    lv_img_dsc->data = (uint8_t *)rgb565;

    return ESP_OK;
}

esp_err_t load_image_from_cache(char *url, lv_img_dsc_t *lv_img_dsc)
{
    esp_err_t ret = ESP_OK;
    FILE *f_img = NULL;
    FILE *f_index = NULL;
    char *img_buffer = NULL;

    f_index = fopen("/sdcard/.img_cache/index", "r");
    ESP_GOTO_ON_FALSE(f_index != NULL, ESP_ERR_NOT_FOUND, err, TAG, "Cannot open cache index file");

    char line[1024];
    while (fgets(line, 1024, f_index))
    {
        char *token = strtok(line, ",");
        if (token)
        {
            if (strcmp(url, token) == 0)
            {
                lv_img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
                lv_img_dsc->header.w = atoi(strtok(NULL, ","));
                lv_img_dsc->header.h = atoi(strtok(NULL, ","));
                lv_img_dsc->data_size = atoi(strtok(NULL, ","));
                char *id = strtok(NULL, ",");
                ESP_LOGI(TAG, "Image from url %s found in cache with id: %s", url, id);
                char img_file_name[128];
                strcpy(img_file_name, "/sdcard/.img_cache/");
                strcat(img_file_name, id);
                f_img = fopen(img_file_name, "r");
                img_buffer = malloc(lv_img_dsc->data_size);
                ESP_GOTO_ON_FALSE(img_buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "Cannot allocate memmory for file load into PSRAM");
                int read = fread(img_buffer, lv_img_dsc->data_size, 1, f_img);
                ESP_GOTO_ON_FALSE(read == 1, ESP_FAIL, err, TAG, "Cannot read the whole file");
                lv_img_dsc->data = (uint8_t *)img_buffer;
            }
        }
    }
    if (f_img == NULL)
    {
        ESP_LOGI(TAG, "Canot find cached image for url %s", url);
        ret = ESP_ERR_NOT_FOUND;
    }
err:
    if (f_index != NULL)
        fclose(f_index);
    if (f_img != NULL)
        fclose(f_img);
    if (ret != ESP_OK && img_buffer != NULL)
        free(img_buffer);
    return ret;
}

esp_err_t save_image_to_cache(char *url, lv_img_dsc_t *lv_img_dsc)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t download_image(char *url, lv_img_dsc_t *lv_img_dsc)
{
    ESP_LOGI(TAG, "Download image from %s", url);

    esp_err_t ret = ESP_OK;
    char *img_buffer = NULL;
    int img_size = 0;

    if (strncmp("file://", url, strlen("file://")) == 0)
    {
        FILE *f = fopen(url + 6, "rb");
        ESP_GOTO_ON_FALSE(f != NULL, ESP_ERR_NOT_FOUND, err, TAG, "Cannot open file");
        int seek = fseek(f, 0, SEEK_END);
        ESP_GOTO_ON_FALSE(seek == 0, ESP_FAIL, err, TAG, "Cannot seek to the end of file");
        img_size = ftell(f);
        seek = fseek(f, 0, SEEK_SET);
        ESP_GOTO_ON_FALSE(seek == 0, ESP_FAIL, err, TAG, "Cannot seek back to the beginning of file");
        img_buffer = malloc(img_size + 1);
        ESP_GOTO_ON_FALSE(img_buffer != NULL, ESP_ERR_NO_MEM, err, TAG, "Cannot allocate memmory for file load into PSRAM");
        int read = fread(img_buffer, img_size, 1, f);
        ESP_GOTO_ON_FALSE(read == 1, ESP_FAIL, err, TAG, "Cannot read the whole file");
        fclose(f);
        img_buffer[img_size] = 0;
        ret = ESP_OK;
    }
    else
    {
        ret = http_client_get(url, &img_buffer, &img_size, HTTP_RESPONSE_MAX_SIZE);
    }
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Cannot download image");

    ret = jpeg_to_rgb565((uint8_t *)img_buffer, img_size, lv_img_dsc);
    if (ret != ESP_OK)
    {
        ret = png_to_rgb565((unsigned char *)img_buffer, img_size, lv_img_dsc);
    }
err:
    if (img_buffer)
    {
        free(img_buffer);
    }
    return ret;
}
