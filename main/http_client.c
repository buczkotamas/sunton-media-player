#include "http_client.h"

#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "HTTP_CLIENT";
static const int perfect_rssi = -20;
static const int worst_rssi = -85;

int http_client_wifi_signal_quality_get(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
    {
        return -1;
    }
    int signal_quality = (100 * (perfect_rssi - worst_rssi) * (perfect_rssi - worst_rssi) -
                          (perfect_rssi - ap.rssi) * (15 * (perfect_rssi - worst_rssi) + 62 * (perfect_rssi - ap.rssi))) /
                         ((perfect_rssi - worst_rssi) * (perfect_rssi - worst_rssi));
    if (signal_quality > 100)
        signal_quality = 100;
    if (signal_quality < 1)
        signal_quality = 0;

    return signal_quality;
}

esp_err_t http_client_get(char *url, char **response_buffer, int *response_size, int max_response_size)
{
    esp_err_t ret = ESP_OK;
    int content_length = 0;
    char *http_buffer = NULL;
    *response_size = 0;
    *response_buffer = NULL;

    if (!heap_caps_check_integrity_all(true))
        ESP_LOGE(TAG, "Heap corruption detected! - http_client_get");
    else
        ESP_LOGI(TAG, "Heap corruption not detected! - http_client_get");

    esp_http_client_config_t config = {
        .url = url,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    ret = esp_http_client_set_method(client, HTTP_METHOD_GET);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to set HTTP method: GET");

    ret = esp_http_client_open(client, 0);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to open HTTP connection");

    content_length = esp_http_client_fetch_headers(client);
    ESP_GOTO_ON_FALSE((content_length >= 0), ESP_ERR_HTTP_FETCH_HEADER, err, TAG, "HTTP client fetch headers failed");
    ESP_GOTO_ON_FALSE((content_length < max_response_size), ESP_ERR_NO_MEM, err, TAG, "Content length bigger then max_response_size");

    int http_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP response code = %d, content-lenght = %d", http_code, content_length);
    ESP_GOTO_ON_FALSE((http_code >= 200 && http_code < 300), ESP_FAIL, err, TAG, "HTTP request returned with error code");

    if (content_length == 0)
    {
        ESP_LOGI(TAG, "HTTP header content length was 0, using max_response_size for response buffer allocation");
        content_length = max_response_size;
    }

    http_buffer = (char *)malloc(content_length + 1);
    ESP_GOTO_ON_FALSE(http_buffer, ESP_ERR_NO_MEM, err, TAG, "Not enough memmory for response buffer");

    int read_len = esp_http_client_read_response(client, http_buffer, content_length);
    ESP_LOGI(TAG, "Read %d bytes from HTTP response", read_len);
    ESP_GOTO_ON_FALSE((read_len > 0), ESP_FAIL, err, TAG, "Cannot read HTTP response content");
    http_buffer[read_len] = 0;

    *response_buffer = http_buffer;
    *response_size = read_len;

err:
    if (ret != ESP_OK && http_buffer)
    {
        free(http_buffer);
    }
    esp_http_client_close(client);
    return ret;
}
