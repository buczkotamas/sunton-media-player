#ifndef DLNA_H
#define DLNA_H

#include "esp_dlna.h"
#include "esp_err.h"

#define DLNA_UNIQUE_DEVICE_NAME "ESP32_DMR_8db0797a"
#define DLNA_DEVICE_UUID "8db0797a-f01a-4949-8f59-51188b181809"
#define DLNA_ROOT_PATH "/rootDesc.xml"

typedef enum
{
    DLNA_EVENT_METADATA = 0,
} dlna_event_t;

typedef void (*dlna_event_cb)(dlna_event_t event, void *subject);

typedef struct
{
    dlna_event_cb callback;
    struct dlna_event_cb_node_t *next;
} dlna_event_cb_node_t;

void dlna_add_event_listener(dlna_event_cb callback);

esp_dlna_handle_t dlna_start();

#endif