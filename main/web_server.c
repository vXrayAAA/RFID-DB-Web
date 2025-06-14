#include "web_server.h"
#include "database.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "WEB_SERVER";

// Declaração da função auxiliar
static void url_decode(const char *src, char *dest, size_t dest_size);

// Declarações das funções handler
esp_err_t index_handler(httpd_req_t *req);
esp_err_t style_handler(httpd_req_t *req);
esp_err_t script_handler(httpd_req_t *req);
esp_err_t api_stats_handler(httpd_req_t *req);
esp_err_t api_cards_handler(httpd_req_t *req);
esp_err_t api_card_add_handler(httpd_req_t *req);
esp_err_t api_card_delete_handler(httpd_req_t *req);
esp_err_t api_logs_handler(httpd_req_t *req);
esp_err_t api_last_card_handler(httpd_req_t *req);
esp_err_t api_scan_handler(httpd_req_t *req);
esp_err_t api_cards_handler(httpd_req_t *req);
esp_err_t api_card_add_handler(httpd_req_t *req);
esp_err_t api_card_delete_handler(httpd_req_t *req);
esp_err_t api_logs_handler(httpd_req_t *req);
esp_err_t api_last_card_handler(httpd_req_t *req);

// Arquivos web incorporados
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[]   asm("_binary_script_js_end");

// Variável global para armazenar último cartão escaneado
static char last_scanned_uid[MAX_UID_LENGTH] = {0};
static bool new_card_available = false;

// Função para definir último cartão escaneado (chamada pelo main)
void web_server_set_last_card(const char *uid) {
    if (uid) {
        ESP_LOGI("WEB_SERVER", "Recebido novo cartão para web: %s", uid);
        strncpy(last_scanned_uid, uid, sizeof(last_scanned_uid) - 1);
        last_scanned_uid[sizeof(last_scanned_uid) - 1] = '\0';
        new_card_available = true;
        ESP_LOGI("WEB_SERVER", "Cartão armazenado para web: %s", last_scanned_uid);
    } else {
        ESP_LOGW("WEB_SERVER", "UID nulo recebido para web");
    }
}

esp_err_t web_server_init(web_server_t *server) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_uri_handlers = 16;
    config.max_resp_headers = 16;
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Iniciando servidor web na porta %d", config.server_port);
    
    if (httpd_start(&server->server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registrando handlers URI");
        
        // Handler para página principal
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &index_uri);
        
        // Handler para CSS
        httpd_uri_t style_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = style_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &style_uri);
        
        // Handler para JavaScript
        httpd_uri_t script_uri = {
            .uri = "/script.js",
            .method = HTTP_GET,
            .handler = script_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &script_uri);
        
        // API Handlers
        httpd_uri_t api_stats_uri = {
            .uri = "/api/stats",
            .method = HTTP_GET,
            .handler = api_stats_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_stats_uri);
        
        httpd_uri_t api_cards_get_uri = {
            .uri = "/api/cards",
            .method = HTTP_GET,
            .handler = api_cards_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_cards_get_uri);
        
        httpd_uri_t api_cards_post_uri = {
            .uri = "/api/cards",
            .method = HTTP_POST,
            .handler = api_card_add_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_cards_post_uri);
        
        httpd_uri_t api_cards_delete_uri = {
            .uri = "/api/cards/*",
            .method = HTTP_DELETE,
            .handler = api_card_delete_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_cards_delete_uri);
          httpd_uri_t api_logs_uri = {
            .uri = "/api/logs",
            .method = HTTP_GET,
            .handler = api_logs_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_logs_uri);
        
        // Adicionar endpoint para último cartão
        httpd_uri_t api_last_card_uri = {
            .uri = "/api/last_card",
            .method = HTTP_GET,
            .handler = api_last_card_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_last_card_uri);
          httpd_uri_t api_scan_uri = {
            .uri = "/api/scan",
            .method = HTTP_GET,
            .handler = api_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server->server, &api_scan_uri);
        
        server->running = true;
        ESP_LOGI(TAG, "Servidor web iniciado com sucesso");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Falha ao iniciar servidor web");
    return ESP_FAIL;
}

esp_err_t web_server_stop(web_server_t *server) {
    if (server->running && server->server) {
        esp_err_t ret = httpd_stop(server->server);
        server->running = false;
        ESP_LOGI(TAG, "Servidor web parado");
        return ret;
    }
    return ESP_OK;
}

esp_err_t index_handler(httpd_req_t *req) {
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

esp_err_t style_handler(httpd_req_t *req) {
    const size_t style_css_size = (style_css_end - style_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)style_css_start, style_css_size);
    return ESP_OK;
}

esp_err_t script_handler(httpd_req_t *req) {
    const size_t script_js_size = (script_js_end - script_js_start);
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)script_js_start, script_js_size);
    return ESP_OK;
}

esp_err_t api_stats_handler(httpd_req_t *req) {
    cJSON *json = cJSON_CreateObject();
    int total_cards = 0, total_accesses = 0;
    
    if (database_get_stats(&total_cards, &total_accesses) == ESP_OK) {
        cJSON_AddNumberToObject(json, "total_cards", total_cards);
        cJSON_AddNumberToObject(json, "total_accesses", total_accesses);
        cJSON_AddBoolToObject(json, "success", true);
    } else {
        cJSON_AddBoolToObject(json, "success", false);
        cJSON_AddStringToObject(json, "message", "Erro ao obter estatísticas");
    }
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t api_cards_handler(httpd_req_t *req) {
    ESP_LOGI("WEB_SERVER", "API /api/cards chamada");
    
    cJSON *json = cJSON_CreateObject();
    cJSON *cards_array = cJSON_CreateArray();
    
    rfid_record_t *records = NULL;
    int count = 0;
    
    ESP_LOGI("WEB_SERVER", "Buscando cartões no banco de dados...");
    esp_err_t result = database_get_all_cards(&records, &count);
    ESP_LOGI("WEB_SERVER", "database_get_all_cards retornou: %s, count: %d", esp_err_to_name(result), count);
    
    if (result == ESP_OK) {
        ESP_LOGI("WEB_SERVER", "Processando %d cartões", count);
        for (int i = 0; i < count; i++) {
            cJSON *card_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(card_obj, "uid", records[i].uid);
            cJSON_AddStringToObject(card_obj, "name", records[i].name);
            cJSON_AddNumberToObject(card_obj, "access_level", records[i].access_level);
            cJSON_AddNumberToObject(card_obj, "first_seen", records[i].first_seen);
            cJSON_AddNumberToObject(card_obj, "last_seen", records[i].last_seen);
            cJSON_AddNumberToObject(card_obj, "access_count", records[i].access_count);
            cJSON_AddItemToArray(cards_array, card_obj);
            
            ESP_LOGI("WEB_SERVER", "Cartão %d: %s (%s)", i, records[i].uid, records[i].name);
        }
        
        if (records) {
            free(records);
        }
        
        cJSON_AddItemToObject(json, "cards", cards_array);
        cJSON_AddBoolToObject(json, "success", true);
    } else {
        ESP_LOGW("WEB_SERVER", "Falha ao obter cartões: %s", esp_err_to_name(result));
        cJSON_Delete(cards_array);
        cJSON_AddBoolToObject(json, "success", false);
        cJSON_AddStringToObject(json, "message", "Erro ao obter cartões");
    }
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t api_card_add_handler(httpd_req_t *req) {
    char content[512];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    cJSON *response = cJSON_CreateObject();
    
    if (json != NULL) {
        cJSON *uid_json = cJSON_GetObjectItem(json, "uid");
        cJSON *name_json = cJSON_GetObjectItem(json, "name");
        cJSON *level_json = cJSON_GetObjectItem(json, "access_level");
        
        if (cJSON_IsString(uid_json) && cJSON_IsString(name_json) && cJSON_IsNumber(level_json)) {
            const char *uid = uid_json->valuestring;
            const char *name = name_json->valuestring;
            uint8_t access_level = (uint8_t)level_json->valueint;
            
            if (database_add_card(uid, name, access_level) == ESP_OK) {
                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Cartão adicionado com sucesso");
                ESP_LOGI(TAG, "Cartão adicionado via API: %s - %s", uid, name);
            } else {
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "message", "Erro ao adicionar cartão no banco de dados");
            }
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "Dados inválidos");
        }
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "JSON inválido");
    }
    
    char *response_string = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_string, strlen(response_string));
    
    free(response_string);
    cJSON_Delete(response);
    if (json) cJSON_Delete(json);
    
    return ESP_OK;
}

esp_err_t api_card_delete_handler(httpd_req_t *req) {
    // Extrair UID da URI
    char uid[64];
    const char *uri = req->uri;
    const char *uid_start = strrchr(uri, '/');
    
    if (uid_start && strlen(uid_start) > 1) {
        strncpy(uid, uid_start + 1, sizeof(uid) - 1);
        uid[sizeof(uid) - 1] = '\0';
        
        // Decodificar URL (substituir %3A por :)
        char decoded_uid[64];
        url_decode(uid, decoded_uid, sizeof(decoded_uid));
        
        cJSON *response = cJSON_CreateObject();
        
        if (database_delete_card(decoded_uid) == ESP_OK) {
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "Cartão excluído com sucesso");
            ESP_LOGI(TAG, "Cartão excluído via API: %s", decoded_uid);
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "message", "Erro ao excluir cartão");
        }
        
        char *response_string = cJSON_Print(response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response_string, strlen(response_string));
        
        free(response_string);
        cJSON_Delete(response);
    } else {
        httpd_resp_send_404(req);
    }
    
    return ESP_OK;
}

esp_err_t api_logs_handler(httpd_req_t *req) {
    cJSON *json = cJSON_CreateObject();
    cJSON *logs_array = cJSON_CreateArray();
    
    access_log_t *logs = NULL;
    int count = 0;
    
    if (database_get_access_logs(&logs, &count, 50) == ESP_OK) { // Últimos 50 logs
        for (int i = 0; i < count; i++) {
            cJSON *log_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(log_obj, "uid", logs[i].uid);
            cJSON_AddStringToObject(log_obj, "action", logs[i].action);
            cJSON_AddNumberToObject(log_obj, "timestamp", logs[i].timestamp);
            cJSON_AddItemToArray(logs_array, log_obj);
        }
        
        if (logs) {
            free(logs);
        }
        
        cJSON_AddItemToObject(json, "logs", logs_array);
        cJSON_AddBoolToObject(json, "success", true);
    } else {
        cJSON_Delete(logs_array);
        cJSON_AddBoolToObject(json, "success", false);
        cJSON_AddStringToObject(json, "message", "Erro ao obter logs");
    }
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t api_scan_handler(httpd_req_t *req) {
    cJSON *response = cJSON_CreateObject();
    
    if (new_card_available && strlen(last_scanned_uid) > 0) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "uid", last_scanned_uid);
        cJSON_AddStringToObject(response, "message", "Cartão detectado");
        
        // Limpar o cartão após ser lido
        new_card_available = false;
        memset(last_scanned_uid, 0, sizeof(last_scanned_uid));
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Nenhum cartão detectado");
    }
    
    char *response_string = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_string, strlen(response_string));
    
    free(response_string);
    cJSON_Delete(response);    return ESP_OK;
}

// Handler para API do último cartão escaneado
esp_err_t api_last_card_handler(httpd_req_t *req) {
    ESP_LOGI("WEB_SERVER", "API /api/last_card chamada");
    
    cJSON *json = cJSON_CreateObject();
    
    if (strlen(last_scanned_uid) > 0 && new_card_available) {
        ESP_LOGI("WEB_SERVER", "Último cartão: %s", last_scanned_uid);
        cJSON_AddStringToObject(json, "uid", last_scanned_uid);
        cJSON_AddBoolToObject(json, "success", true);
        
        // Buscar informações completas do cartão se disponível
        rfid_record_t card_record;
        if (database_get_card(last_scanned_uid, &card_record) == ESP_OK) {
            cJSON_AddStringToObject(json, "name", card_record.name);
            cJSON_AddNumberToObject(json, "access_level", card_record.access_level);
            cJSON_AddNumberToObject(json, "access_count", card_record.access_count);
        } else {
            cJSON_AddStringToObject(json, "name", "Cartão novo");
            cJSON_AddNumberToObject(json, "access_level", 1);
            cJSON_AddNumberToObject(json, "access_count", 0);
        }
    } else {
        ESP_LOGI("WEB_SERVER", "Nenhum cartão escaneado ainda");
        cJSON_AddBoolToObject(json, "success", false);
        cJSON_AddStringToObject(json, "message", "Nenhum cartão escaneado");
    }
    
    char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    return ESP_OK;
}

// Função auxiliar para decodificar URL
static void url_decode(const char *src, char *dest, size_t dest_size) {
    size_t src_len = strlen(src);
    size_t dest_idx = 0;
    
    for (size_t i = 0; i < src_len && dest_idx < dest_size - 1; i++) {
        if (src[i] == '%' && i + 2 < src_len) {
            // Decodificar caractere hexadecimal
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dest[dest_idx++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            dest[dest_idx++] = src[i];
        }
    }
    dest[dest_idx] = '\0';
}
