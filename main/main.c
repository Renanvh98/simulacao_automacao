#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_http_server.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

#define LED_PIN 2

char ligada_resp[] = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
                 "body { font-family: Arial, sans-serif; background-color: #eceff1; margin: 0; padding: 0; text-align: center; }"
                 "h1 { color: #37474f; padding: 2vh; }"
                 ".button { display: inline-block; background-color: #0288d1; border: none; border-radius: 8px; color: white; padding: 20px 50px; text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer; transition: background-color 0.3s; }"
                 ".button2 { background-color: #d32f2f; }"
                 ".button:hover { background-color: #039be5; }"
                 ".button2:hover { background-color: #e53935; }"
                 ".content { padding: 30px; }"
                 ".card-grid { max-width: 900px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }"
                 ".card { background-color: #ffffff; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.1); border-radius: 12px; padding: 20px; }"
                 ".card-title { font-size: 1.4rem; font-weight: bold; color: #0288d1; margin-bottom: 10px; }"
                 "</style><title>Automação Residencial</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                 "<link rel=\"icon\" href=\"data:,\">"
                 "<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\"></head>"
                 "<body><h1>Automação de Lâmpada da Sala</h1><div class=\"content\"><div class=\"card-grid\"><div class=\"card\"><p><i class=\"fas fa-lightbulb fa-2x\" style=\"color:#ffd600;\"></i> <strong>Lâmpada da Sala</strong></p>"
                 "<p>Estado da Lâmpada: <strong>LIGADA</strong></p>"
                 "<p><a href=\"/ligada\"><button class=\"button\">Ligar</button></a><a href=\"/desligada\"><button class=\"button button2\">Desligar</button></a></p>"
                 "</div></div></div></body></html>";




char desligada_resp[] = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
                  "body { font-family: Arial, sans-serif; background-color: #eceff1; margin: 0; padding: 0; text-align: center; }"
                  "h1 { color: #37474f; padding: 2vh; }"
                  ".button { display: inline-block; background-color: #0288d1; border: none; border-radius: 8px; color: white; padding: 20px 50px; text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer; transition: background-color 0.3s; }"
                  ".button2 { background-color: #d32f2f; }"
                  ".button:hover { background-color: #039be5; }"
                  ".button2:hover { background-color: #e53935; }"
                  ".content { padding: 30px; }"
                  ".card-grid { max-width: 900px; margin: 0 auto; display: grid; grid-gap: 1rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }"
                  ".card { background-color: #ffffff; box-shadow: 2px 2px 12px 1px rgba(0,0,0,0.1); border-radius: 12px; padding: 20px; }"
                  ".card-title { font-size: 1.4rem; font-weight: bold; color: #0288d1; margin-bottom: 10px; }"
                  "</style><title>Automação de Lâmpada da Sala</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<link rel=\"icon\" href=\"data:,\">"
                  "<link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\"></head>"
                  "<body><h1>Automação de Lâmpada da Sala</h1><div class=\"content\"><div class=\"card-grid\"><div class=\"card\"><p><i class=\"fas fa-lightbulb fa-2x\" style=\"color:#ffd600;\"></i> <strong>Lâmpada da Sala</strong></p>"
                  "<p>Estado da Lâmpada: <strong>DESLIGADA</strong></p>"
                  "<p><a href=\"/ligada\"><button class=\"button\">Ligar</button></a><a href=\"/desligada\"><button class=\"button button2\">Desligar</button></a></p>"
                  "</div></div></div></body></html>";




static const char *TAG = "espressif"; // TAG for debug
int led_state = 0;
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void connect_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t send_web_page(httpd_req_t *req)
{
    int response;
    if (led_state == 0)
        response = httpd_resp_send(req, desligada_resp, HTTPD_RESP_USE_STRLEN);
    else
        response = httpd_resp_send(req, ligada_resp, HTTPD_RESP_USE_STRLEN);
    return response;
}
esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

esp_err_t ligada_handler(httpd_req_t *req)
{
    gpio_set_level(LED_PIN, 1);
    led_state = 1;
    return send_web_page(req);
}

esp_err_t desligada_handler(httpd_req_t *req)
{
    gpio_set_level(LED_PIN, 0);
    led_state = 0;
    return send_web_page(req);
}

httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL};

httpd_uri_t uri_ligada = {
    .uri = "/ligada",
    .method = HTTP_GET,
    .handler = ligada_handler,
    .user_ctx = NULL};

httpd_uri_t uri_desligada = {
    .uri = "/desligada",
    .method = HTTP_GET,
    .handler = desligada_handler,
    .user_ctx = NULL};

httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_ligada);
        httpd_register_uri_handler(server, &uri_desligada);
    }

    return server;
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    connect_wifi();

    esp_rom_gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    led_state = 0;
    ESP_LOGI(TAG, "LED Control Web Server is running ... ...\n");
    setup_server();
}