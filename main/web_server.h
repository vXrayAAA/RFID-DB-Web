#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "esp_err.h"

// Configurações do servidor web
#define WEB_SERVER_PORT 80
#define MAX_JSON_RESPONSE_SIZE 4096

typedef struct {
    httpd_handle_t server;
    bool running;
} web_server_t;

// Funções do servidor web
esp_err_t web_server_init(web_server_t *server);
esp_err_t web_server_stop(web_server_t *server);

// Handlers HTTP
esp_err_t index_handler(httpd_req_t *req);
esp_err_t style_handler(httpd_req_t *req);
esp_err_t script_handler(httpd_req_t *req);
esp_err_t api_cards_handler(httpd_req_t *req);
esp_err_t api_logs_handler(httpd_req_t *req);
esp_err_t api_stats_handler(httpd_req_t *req);
esp_err_t api_card_add_handler(httpd_req_t *req);
esp_err_t api_card_delete_handler(httpd_req_t *req);
esp_err_t api_scan_handler(httpd_req_t *req);

// Função para definir último cartão escaneado
void web_server_set_last_card(const char *uid);

#endif // WEB_SERVER_H
