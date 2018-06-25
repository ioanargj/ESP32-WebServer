#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include <string.h>

#include <test_component.h>
#include <http_server.h>

#define WIFI_SSID       "ESP32_WIFI"
#define WIFI_PSK        "password1234"
#define WIFI_MAX_CONN   5

#define CONNECTED_BIT   BIT0

static bool isConnected = false;

char *website = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><title>ESP WEBSITE</title></head><body><form action=\"/update.html\" method=\"POST\">SSID:<br><input type=\"text\" name=\"ssid\"/><br>Password:<br><input type=\"password\" name=\"psk\"/><br><input type=\"submit\" value=\"Submit\"/></form></body></html>";

static EventGroupHandle_t wifi_event_group;

void wifi_init_softap(void);
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    const char *TAG = "EVENT_HANDLER";
    switch (event->event_id) {
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                    MAC2STR(event->event_info.sta_connected.mac),
                    event->event_info.sta_connected.aid);
            break;

        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                    MAC2STR(event->event_info.sta_disconnected.mac),
                    event->event_info.sta_disconnected.aid);
            break;

        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            isConnected = true;
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            isConnected = false;
            printf("CONNECTION TO WIFI NETWORK FAILED\n");
            // esp_wifi_connect();
            wifi_init_softap();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;

        default:
            break;
    }
    return ESP_OK;
}

void wifi_init_softap(void)
{
    esp_wifi_stop();
    wifi_config_t config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PSK,
            .max_connection = WIFI_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    ESP_LOGI("wifi_init_softap", "wifi_init_softap finished; SSID:%s password:%s",
             WIFI_SSID, WIFI_PSK);
}

void handle_wifi_creds(char *params) {
    // printf("PARSING WIFI CREDS\n");

    char *ssid_scan = scan_url_encoded(params, "ssid");
    printf("ssid_scan test: %s\n", ssid_scan);

    char *psk_scan = scan_url_encoded(params, "psk");
    printf("psk_scan test: %s\n", psk_scan);

    ESP_ERROR_CHECK( esp_wifi_stop() );
    
    wifi_config_t config = {
		.sta = {
			.ssid = "CRAP SSID WITH LOTS OF CHARS",
			.password = "CRAP PSK WITH LOTS OF CHARS",
		},
	};
    strcpy((char *)config.sta.ssid, ssid_scan);
    strcpy((char *)config.sta.password, psk_scan);

    printf("Connecting to network: SSID=%s; PSK=%s\n", config.sta.ssid, config.sta.password);

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &config) );
    ESP_ERROR_CHECK( esp_wifi_start() );   
}

void http_server_callback(struct http_request *request, char *buffer) 
{
    if (request->method == HTTP_POST) {
        handle_wifi_creds(request->body);
    }
    strcpy(buffer, website);
}

void app_main(void) 
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(  esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    wifi_init_softap();

    test_component_func();

    http_server_set_client_callback(&http_server_callback);
    http_server_start();
}

