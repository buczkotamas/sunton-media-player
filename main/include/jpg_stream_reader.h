#ifndef _JPG_STREAM_READER_H
#define _JPG_STREAM_READER_H

#include "esp_err.h"
#include "jpeg_decoder.h"

typedef enum
{
    JPG_STREAM_EVENT_OPEN = 0,
    JPG_STREAM_EVENT_CLOSE = 1,
    JPG_STREAM_EVENT_FRAME = 2,
    JPG_STREAM_EVENT_ERROR = 3,
    JPG_STREAM_EVENT_STATUS = 4,
} jpg_stream_event_t;

typedef void (*jpg_stream_event_cb)(jpg_stream_event_t event, uint16_t *data, esp_jpeg_image_output_t *img);

esp_err_t jpg_stream_open(char *url, jpg_stream_event_cb event_cb);
esp_err_t jpg_stream_close(void);

#endif