// Stand-in for ESP-IDF's esp_http_client.h (with the esp_err.h codes it
// brings in): only what the units under test use. Tests define the functions.
#pragma once
#include <cstdint>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_HTTP_BASE 0x7000
#define ESP_ERR_HTTP_CONNECT (ESP_ERR_HTTP_BASE + 2)
#define ESP_ERR_HTTP_EAGAIN (ESP_ERR_HTTP_BASE + 7)
const char *esp_err_to_name(esp_err_t e);

typedef enum { HTTP_METHOD_GET = 0, HTTP_METHOD_POST } esp_http_client_method_t;
typedef struct {
  const char *url;
  esp_http_client_method_t method;
  int timeout_ms;
  const char *cert_pem;
  bool disable_auto_redirect;
  const char *user_agent;
  int buffer_size;
  int buffer_size_tx;
} esp_http_client_config_t;
typedef struct fake_client *esp_http_client_handle_t;

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value);
esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len);
int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
