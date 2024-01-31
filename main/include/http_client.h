#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"

int http_client_wifi_signal_quality_get(void);
esp_err_t http_client_get(char *url, char **response, int *response_size, int max_response_size);

#endif