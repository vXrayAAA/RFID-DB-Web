#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool s_is_connected = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentando reconectar ao AP (%d/10)", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        s_is_connected = false;
        ESP_LOGI(TAG, "Falha ao conectar ao AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtido:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void) {
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Criar apenas interface STA (Station)
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
    
    ESP_LOGI(TAG, "WiFi Manager inicializado");
    
    return ESP_OK;
}

esp_err_t wifi_manager_start_ap(const char *ssid, const char *password) {
    wifi_config_t wifi_config = {
        .ap = {
            .channel = WIFI_CHANNEL,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(ssid);
    
    if (password && strlen(password) > 0) {
        strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Modo AP iniciado. SSID: %s, Canal: %d", ssid, WIFI_CHANNEL);
    
    return ESP_OK;
}

esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password) {
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Conectando ao AP SSID: %s", ssid);
      // Aguardar conexão ou falha (com timeout de 30 segundos)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(30000)); // Timeout de 30 segundos
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao AP SSID: %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Falha ao conectar ao AP SSID: %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Evento inesperado");
        return ESP_FAIL;
    }
}

esp_err_t wifi_manager_get_ip(char *ip_str, size_t len) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return ESP_FAIL;
    }
    
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret == ESP_OK) {
        snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    }
    
    return ret;
}

bool wifi_manager_is_connected(void) {
    return s_is_connected;
}
