#ifndef DATABASE_H
#define DATABASE_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include "esp_err.h"

// Definições de tamanhos
#define MAX_UID_LENGTH 32
#define MAX_NAME_LENGTH 64
#define MAX_ACTION_LENGTH 16

// Níveis de acesso
#define ACCESS_LEVEL_USER     1
#define ACCESS_LEVEL_ADMIN    2
#define ACCESS_LEVEL_MASTER   3

typedef struct {
    uint32_t id;
    char uid[MAX_UID_LENGTH];
    char name[MAX_NAME_LENGTH];
    time_t first_seen;
    time_t last_seen;
    uint32_t access_count;
    uint8_t access_level;
} rfid_record_t;

typedef struct {
    uint32_t id;
    char uid[MAX_UID_LENGTH];
    time_t timestamp;
    char action[MAX_ACTION_LENGTH];
} access_log_t;

// Funções do banco de dados
esp_err_t database_init(void);
esp_err_t database_close(void);

// Operações com cartões RFID
esp_err_t database_add_card(const char *uid, const char *name, uint8_t access_level);
esp_err_t database_update_card_access(const char *uid);
esp_err_t database_get_card(const char *uid, rfid_record_t *record);
esp_err_t database_delete_card(const char *uid);
esp_err_t database_get_all_cards(rfid_record_t **records, int *count);

// Log de acesso
esp_err_t database_add_access_log(const char *uid, const char *action);
esp_err_t database_get_access_logs(access_log_t **logs, int *count, int limit);

// Estatísticas
esp_err_t database_get_stats(int *total_cards, int *total_accesses);

#endif // DATABASE_H
