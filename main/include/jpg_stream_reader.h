#ifndef _JPG_STREAM_READER_H
#define _JPG_STREAM_READER_H

#include "esp_err.h"
#include "jpeg_decoder.h"

#define HEADER_BUFFER_SIZE 64
#define OUTPUT_IMAGE_W 240
#define OUTPUT_IMAGE_H 240

typedef struct buffer_info_s
{
    char *data;
    size_t size;
    int id;
    esp_jpeg_image_output_t img;
} buffer_info_t;

typedef void (*jpg_stream_on_frame_cb)(uint16_t *data, esp_jpeg_image_output_t img);

esp_err_t jpg_stream_open(char *url, jpg_stream_on_frame_cb on_frame_cb);
esp_err_t jpg_stream_close(void);

#endif