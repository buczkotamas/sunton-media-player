#ifndef DLNA_H
#define DLNA_H

#include "esp_dlna.h"
#include "esp_err.h"

#define DLNA_UNIQUE_DEVICE_NAME "ESP32_DMR_8db0797a"
#define DLNA_DEVICE_UUID "8db0797a-f01a-4949-8f59-51188b181809"
#define DLNA_ROOT_PATH "/rootDesc.xml"

esp_dlna_handle_t dlna_start();

#endif