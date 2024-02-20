#ifndef _JPG_STREAM_READER_H
#define _JPG_STREAM_READER_H

#include "esp_err.h"
#include "jpeg_decoder.h"

typedef void (*jpg_stream_on_frame_cb)(uint16_t *data, esp_jpeg_image_output_t img);

esp_err_t jpg_stream_open(char *url, jpg_stream_on_frame_cb on_frame_cb);
esp_err_t jpg_stream_close(void);

#endif