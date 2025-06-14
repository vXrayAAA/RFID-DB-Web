#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"

// Includes dos componentes do projeto
#include "rc522.h"
#include "database.h"
#include "web_server.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

// Variáveis globais
static web_server_t web_server;
static rc522_handle_t rc522_handle;
static bool system_ready = false;

// Task para leitura do RFID
void rfid_task(void *pvParameters) {
    char uid_str[MAX_UID_LENGTH];
    rfid_record_t card_record;
    
    ESP_LOGI(TAG, "Task RFID iniciada");
    
    // Log inicial para debug
    ESP_LOGI(TAG, "RC522 handle inicializado: %s", rc522_handle.initialized ? "SIM" : "NÃO");
    
    while (1) {
        // Debug: Log periodicamente para mostrar que a task está rodando
        static int debug_counter = 0;
        if (debug_counter % 500 == 0) { // A cada 50 segundos (500 * 100ms)
            ESP_LOGI(TAG, "Task RFID ativa - verificando cartões...");
        }
        debug_counter++;
        
        // Aguardar cartão ser detectado
        int card_status = rc522_card_present(&rc522_handle);
        if (card_status == RC522_OK) {
            ESP_LOGI(TAG, "Cartão detectado pelo RC522!");
            
            // Aguardar um pouco antes de tentar ler
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Tentar ler UID até 3 vezes
            int read_attempts = 0;
            esp_err_t read_result = ESP_FAIL;
            
            while (read_attempts < 3 && read_result != ESP_OK) {
                read_result = rc522_read_card_uid(&rc522_handle, uid_str, sizeof(uid_str));
                if (read_result != ESP_OK) {
                    read_attempts++;
                    ESP_LOGW(TAG, "Tentativa %d/3 de leitura falhou", read_attempts);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
            
            if (read_result == ESP_OK) {
                ESP_LOGI(TAG, "Cartão detectado: %s", uid_str);
                
                // Informar ao servidor web sobre o novo cartão
                web_server_set_last_card(uid_str);
                
                // Verificar se cartão está no banco de dados
                if (database_get_card(uid_str, &card_record) == ESP_OK) {
                    // Cartão já cadastrado - atualizar acesso
                    ESP_LOGI(TAG, "Acesso autorizado para: %s (%s)", card_record.name, uid_str);
                    database_update_card_access(uid_str);
                    database_add_access_log(uid_str, "ACCESS_GRANTED");
                    
                    // Aqui você pode adicionar ação para acesso autorizado
                    // Ex: acionar relé, LED verde, etc.
                    
                } else {
                    // Cartão novo - adicionar automaticamente ao sistema
                    char default_name[64];
                    snprintf(default_name, sizeof(default_name), "Cartao_%s", uid_str);
                    
                    ESP_LOGI(TAG, "Novo cartão detectado: %s - Adicionando ao sistema", uid_str);
                    
                    // Adicionar cartão com nível de usuário padrão
                    if (database_add_card(uid_str, default_name, ACCESS_LEVEL_USER) == ESP_OK) {
                        ESP_LOGI(TAG, "Cartão %s adicionado com sucesso como: %s", uid_str, default_name);
                        database_add_access_log(uid_str, "CARD_ADDED");
                        
                        // Conceder acesso imediatamente após adicionar
                        database_update_card_access(uid_str);
                        database_add_access_log(uid_str, "ACCESS_GRANTED");
                        
                        ESP_LOGI(TAG, "Acesso concedido para novo cartão: %s", uid_str);
                    } else {
                        ESP_LOGW(TAG, "Falha ao adicionar cartão: %s", uid_str);
                        database_add_access_log(uid_str, "ADD_FAILED");
                    }
                }
                
                // Aguardar cartão ser removido para evitar leituras duplicadas
                while (rc522_card_present(&rc522_handle)) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            } else {
                ESP_LOGW(TAG, "Falha ao ler UID do cartão detectado");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Verificar a cada 100ms
    }
}

// Task para monitoramento do sistema
void system_monitor_task(void *pvParameters) {
    static int total_cards = 0, total_accesses = 0;
    static int loop_counter = 0;
    
    ESP_LOGI(TAG, "Task de monitoramento iniciada");
    
    while (1) {
        loop_counter++;
        
        // Reduzir frequência de verificação para evitar sobrecarga
        if (system_ready && (loop_counter % 6 == 0)) { // A cada 3 minutos (6 * 30s)
            // Obter estatísticas do sistema
            if (database_get_stats(&total_cards, &total_accesses) == ESP_OK) {
                ESP_LOGI(TAG, "Sistema ativo - Cartões: %d, Acessos: %d", total_cards, total_accesses);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // Log a cada 30 segundos
    }
}

// Função para configurar SNTP
void configure_sntp(void) {
    ESP_LOGI(TAG, "Configurando SNTP...");
    
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_init();
    
    // Aguardar sincronização
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Aguardando sincronização SNTP... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year > (2020 - 1900)) {
        ESP_LOGI(TAG, "SNTP sincronizado: %04d-%02d-%02d %02d:%02d:%02d",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGW(TAG, "SNTP não sincronizado, usando hora local");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Sistema RFID Database iniciando ===");
    
    // 1. Inicializar NVS
    ESP_LOGI(TAG, "Inicializando NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 2. Inicializar banco de dados
    ESP_LOGI(TAG, "Inicializando banco de dados...");
    ret = database_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar banco de dados: %s", esp_err_to_name(ret));
        return;
    }
    
    // 3. Inicializar Wi-Fi
    ESP_LOGI(TAG, "Inicializando Wi-Fi...");
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar Wi-Fi: %s", esp_err_to_name(ret));
        return;
    }
    
    // 4. Conectar ao Wi-Fi em modo STA
    ESP_LOGI(TAG, "Conectando ao Wi-Fi: %s", WIFI_SSID);
    ret = wifi_manager_connect_sta(WIFI_SSID, WIFI_PASS);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi conectado com sucesso!");
    } else {
        ESP_LOGE(TAG, "Falha ao conectar ao Wi-Fi. Verifique as credenciais.");
        ESP_LOGE(TAG, "SSID: %s", WIFI_SSID);
        ESP_LOGE(TAG, "Sistema continuará sem Wi-Fi...");
    }
    
    // 5. Configurar SNTP para sincronização de horário
    configure_sntp();
    
    // 6. Inicializar servidor web
    ESP_LOGI(TAG, "Inicializando servidor web...");
    ret = web_server_init(&web_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar servidor web: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Servidor web iniciado na porta %d", WEB_SERVER_PORT);
    
    // 7. Inicializar RC522
    ESP_LOGI(TAG, "Inicializando leitor RFID RC522...");
    ret = rc522_init(&rc522_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar RC522: %s", esp_err_to_name(ret));
        return;
    }
    
    // Testar comunicação com RC522
    ESP_LOGI(TAG, "Testando comunicação com RC522...");
    ret = rc522_test_communication(&rc522_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Teste de comunicação RC522 falhou - verifique conexões");
    }
    
    // 8. Sistema pronto para reconhecer qualquer cartão RFID
    ESP_LOGI(TAG, "Sistema configurado para reconhecer automaticamente cartões RFID");
    
    // 9. Criar tasks do sistema
    ESP_LOGI(TAG, "Criando tasks do sistema...");
    
    // Task para leitura RFID
    xTaskCreate(rfid_task, "rfid_task", 4096, NULL, 5, NULL);
    
    // Task para monitoramento (aumentar stack)
    xTaskCreate(system_monitor_task, "monitor_task", 4096, NULL, 3, NULL);
    
    // Sistema pronto
    system_ready = true;
    ESP_LOGI(TAG, "=== Sistema RFID Database pronto! ===");
    
    // Obter IP para mostrar no log
    char ip_str[16];
    if (wifi_manager_get_ip(ip_str, sizeof(ip_str)) == ESP_OK) {
        ESP_LOGI(TAG, "Acesse a interface web em: http://%s:%d", ip_str, WEB_SERVER_PORT);
    }
    
    // Loop principal
    while (1) {
        // Verificar se todos os componentes estão funcionando
        if (!wifi_manager_is_connected()) {
            ESP_LOGW(TAG, "Wi-Fi desconectado, tentando reconectar...");
            system_ready = false;
            
            // Aguardar reconexão
            while (!wifi_manager_is_connected()) {
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            
            system_ready = true;
            ESP_LOGI(TAG, "Wi-Fi reconectado!");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Verificar a cada 10 segundos
    }
}