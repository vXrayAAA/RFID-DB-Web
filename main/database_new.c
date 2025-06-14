#include "database.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

static nvs_handle_t nvs_database_handle;

#define CARD_COUNT_KEY "card_count"
#define CARD_PREFIX "card_"
#define LOG_COUNT_KEY "log_count"
#define LOG_PREFIX "log_"

esp_err_t database_init(void) {
    esp_err_t ret;
    
    // Inicializar NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Abrir namespace NVS
    ret = nvs_open("rfid_storage", NVS_READWRITE, &nvs_database_handle);
    if (ret != ESP_OK) {
        printf("Erro ao abrir NVS: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    printf("Banco de dados NVS inicializado com sucesso\n");
    return ESP_OK;
}

esp_err_t database_close(void) {
    nvs_close(nvs_database_handle);
    printf("Banco de dados fechado\n");
    return ESP_OK;
}

esp_err_t database_add_card(const char *uid, const char *name, uint8_t access_level) {
    if (!uid || !name) {
        return ESP_ERR_INVALID_ARG;
    }
      // Verificar se cartão já existe
    rfid_record_t existing_card;
    if (database_get_card(uid, &existing_card) == ESP_OK) {
        printf("Cartão %s já existe\n", uid);
        return ESP_FAIL; // Mudado de ESP_ERR_DUPLICATE_KEY para ESP_FAIL
    }
    
    // Obter contagem atual de cartões
    uint32_t card_count = 0;
    nvs_get_u32(nvs_database_handle, CARD_COUNT_KEY, &card_count);
      // Criar novo registro
    rfid_record_t new_card = {0};
    new_card.id = card_count + 1;
    strncpy(new_card.uid, uid, MAX_UID_LENGTH - 1);
    strncpy(new_card.name, name, MAX_NAME_LENGTH - 1);
    new_card.access_level = access_level;
    new_card.first_seen = time(NULL);
    new_card.last_seen = time(NULL);
    new_card.access_count = 0;
      // Salvar cartão no NVS usando índice numérico
    char key[32];
    snprintf(key, sizeof(key), "%s%" PRIu32, CARD_PREFIX, card_count);
    esp_err_t ret = nvs_set_blob(nvs_database_handle, key, &new_card, sizeof(new_card));
    if (ret != ESP_OK) {
        printf("Erro ao salvar cartão: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    // Atualizar contagem
    card_count++;
    ret = nvs_set_u32(nvs_database_handle, CARD_COUNT_KEY, card_count);
    if (ret != ESP_OK) {
        printf("Erro ao atualizar contagem: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    // Commit mudanças
    ret = nvs_commit(nvs_database_handle);
    if (ret != ESP_OK) {
        printf("Erro ao commit: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    printf("Cartão adicionado: %s - %s\n", uid, name);
    return ESP_OK;
}

esp_err_t database_update_card_access(const char *uid) {
    if (!uid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Buscar cartão existente
    rfid_record_t card;
    esp_err_t ret = database_get_card(uid, &card);
    if (ret != ESP_OK) {
        return ret;
    }
      // Atualizar informações de acesso
    card.last_seen = time(NULL);
    card.access_count++;
    
    // Buscar a chave correta do cartão no NVS
    uint32_t card_count = 0;
    esp_err_t ret2 = nvs_get_u32(nvs_database_handle, CARD_COUNT_KEY, &card_count);
    if (ret2 != ESP_OK) {
        return ret2;
    }
    
    // Buscar pelo UID para encontrar a chave correta
    for (uint32_t i = 0; i < card_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%" PRIu32, CARD_PREFIX, i);
        
        rfid_record_t temp_card;
        size_t required_size = sizeof(rfid_record_t);
        esp_err_t ret3 = nvs_get_blob(nvs_database_handle, key, &temp_card, &required_size);
        if (ret3 == ESP_OK && strcmp(temp_card.uid, uid) == 0) {
            // Encontrou o cartão, atualizar
            ret = nvs_set_blob(nvs_database_handle, key, &card, sizeof(card));
            if (ret != ESP_OK) {
                printf("Erro ao atualizar cartão: %s\n", esp_err_to_name(ret));
                return ret;
            }
            break;
        }
    }
    
    ret = nvs_commit(nvs_database_handle);
    if (ret != ESP_OK) {
        printf("Erro ao commit: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t database_get_card(const char *uid, rfid_record_t *record) {
    if (!uid || !record) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Obter número de cartões armazenados
    uint32_t card_count = 0;
    esp_err_t ret = nvs_get_u32(nvs_database_handle, CARD_COUNT_KEY, &card_count);
    if (ret != ESP_OK || card_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Buscar pelo UID em todos os registros
    for (uint32_t i = 0; i < card_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%" PRIu32, CARD_PREFIX, i);
        
        size_t required_size = sizeof(rfid_record_t);
        ret = nvs_get_blob(nvs_database_handle, key, record, &required_size);
        if (ret == ESP_OK && strcmp(record->uid, uid) == 0) {
            return ESP_OK; // Encontrado!
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t database_delete_card(const char *uid) {
    if (!uid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char key[32];
    snprintf(key, sizeof(key), "%s%s", CARD_PREFIX, uid);
    
    esp_err_t ret = nvs_erase_key(nvs_database_handle, key);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        printf("Erro ao deletar cartão: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_commit(nvs_database_handle);
    if (ret != ESP_OK) {
        printf("Erro ao commit: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    printf("Cartão deletado: %s\n", uid);
    return ESP_OK;
}

esp_err_t database_get_all_cards(rfid_record_t **records, int *count) {
    if (!records || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret;
    size_t required_size = sizeof(uint32_t);
    uint32_t card_count = 0;
      // Obter número de cartões armazenados
    ret = nvs_get_u32(nvs_database_handle, CARD_COUNT_KEY, &card_count);
    printf("DEBUG: Card count from NVS: %" PRIu32 ", ret: %s\n", card_count, esp_err_to_name(ret));
    
    if (ret != ESP_OK || card_count == 0) {
        printf("DEBUG: Nenhum cartão encontrado ou erro ao ler contagem\n");
        *records = NULL;
        *count = 0;
        return ESP_OK;
    }
      // Alocar memória para os registros
    *records = malloc(card_count * sizeof(rfid_record_t));
    if (!*records) {
        printf("DEBUG: Erro ao alocar memória\n");
        return ESP_ERR_NO_MEM;
    }
    
    printf("DEBUG: Alocou memória para %" PRIu32 " cartões\n", card_count);
      int valid_cards = 0;
    for (uint32_t i = 0; i < card_count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%" PRIu32, CARD_PREFIX, i);
        
        required_size = sizeof(rfid_record_t);
        ret = nvs_get_blob(nvs_database_handle, key, &(*records)[valid_cards], &required_size);
        printf("DEBUG: Tentando ler chave %s, ret: %s\n", key, esp_err_to_name(ret));
        
        if (ret == ESP_OK) {
            printf("DEBUG: Cartão encontrado: %s (%s)\n", (*records)[valid_cards].uid, (*records)[valid_cards].name);
            valid_cards++;
        }
    }
    
    *count = valid_cards;
    printf("DEBUG: Total de cartões válidos: %d\n", valid_cards);
    printf("Recuperados %d cartões do NVS\n", valid_cards);
    return ESP_OK;
}

esp_err_t database_add_access_log(const char *uid, const char *action) {
    if (!uid || !action) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Obter contagem atual de logs
    uint32_t log_count = 0;
    nvs_get_u32(nvs_database_handle, LOG_COUNT_KEY, &log_count);
    
    // Criar novo log
    access_log_t new_log = {0};
    new_log.id = log_count + 1;
    strncpy(new_log.uid, uid, MAX_UID_LENGTH - 1);
    strncpy(new_log.action, action, 32 - 1);
    new_log.timestamp = time(NULL);
    
    // Salvar log no NVS (manter apenas os últimos 50 logs)
    char key[32];
    uint32_t log_index = log_count % 50; // Circular buffer
    snprintf(key, sizeof(key), "%s%" PRIu32, LOG_PREFIX, log_index);
    
    esp_err_t ret = nvs_set_blob(nvs_database_handle, key, &new_log, sizeof(new_log));
    if (ret != ESP_OK) {
        printf("Erro ao salvar log: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    // Atualizar contagem
    log_count++;
    ret = nvs_set_u32(nvs_database_handle, LOG_COUNT_KEY, log_count);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_database_handle);
    }
    
    printf("Log de acesso: %s - %s\n", uid, action);
    return ESP_OK;
}

esp_err_t database_get_access_logs(access_log_t **logs, int *count, int limit) {
    if (!logs || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Obter contagem total de logs
    uint32_t total_logs = 0;
    esp_err_t ret = nvs_get_u32(nvs_database_handle, LOG_COUNT_KEY, &total_logs);
    if (ret != ESP_OK || total_logs == 0) {
        *logs = NULL;
        *count = 0;
        return ESP_OK;
    }
    
    // Calcular quantos logs recuperar (máximo 50 devido ao circular buffer)
    int logs_to_get = (total_logs > 50) ? 50 : total_logs;
    if (limit > 0 && limit < logs_to_get) {
        logs_to_get = limit;
    }
    
    // Alocar memória para os logs
    *logs = malloc(logs_to_get * sizeof(access_log_t));
    if (!*logs) {
        return ESP_ERR_NO_MEM;
    }
    
    int valid_logs = 0;
    for (int i = 0; i < logs_to_get && valid_logs < logs_to_get; i++) {
        char key[32];
        snprintf(key, sizeof(key), "%s%d", LOG_PREFIX, i);
        
        size_t required_size = sizeof(access_log_t);
        ret = nvs_get_blob(nvs_database_handle, key, &(*logs)[valid_logs], &required_size);
        if (ret == ESP_OK) {
            valid_logs++;
        }
    }
    
    *count = valid_logs;
    printf("Recuperados %d logs do NVS\n", valid_logs);
    return ESP_OK;
}

esp_err_t database_get_stats(int *total_cards, int *total_accesses) {
    if (!total_cards || !total_accesses) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Obter contagem de cartões
    uint32_t card_count = 0;
    nvs_get_u32(nvs_database_handle, CARD_COUNT_KEY, &card_count);
    
    // Obter contagem de logs/acessos
    uint32_t log_count = 0;
    nvs_get_u32(nvs_database_handle, LOG_COUNT_KEY, &log_count);
    
    *total_cards = card_count;
    *total_accesses = log_count;
    
    return ESP_OK;
}
