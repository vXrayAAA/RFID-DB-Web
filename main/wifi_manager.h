#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"


// #define CONFIG_ESP_WIFI_SSID                         // SSID do AP.
// 
// #define CONFIG_ESP_WIFI_PASSWORD                    // Senha do AP.


// Configurações Wi-Fi (altere conforme necessário)
#define WIFI_SSID      "09382@SmartNet-2.4ghz_exit" 
#define WIFI_PASS      "11434819"  
#define WIFI_CHANNEL   1
#define MAX_STA_CONN   4

typedef enum {
    WIFI_MODE_CONFIG_STA,
    WIFI_MODE_CONFIG_AP,
    WIFI_MODE_CONFIG_APSTA
} wifi_mode_config_t;

typedef struct {
    char ssid[32];
    char password[64];
    uint8_t channel;
    uint8_t max_connections;
    bool is_connected;
    wifi_mode_config_t mode;
} wifi_manager_config_t;

// Funções Wi-Fi
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_ap(const char *ssid, const char *password);
esp_err_t wifi_manager_connect_sta(const char *ssid, const char *password);
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len);
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
