idf_component_register(SRCS "main.c" "rc522.c" "database_new.c" "web_server.c" "wifi_manager.c"
                       INCLUDE_DIRS "."
                       EMBED_FILES "web/index.html" "web/style.css" "web/script.js"
                       REQUIRES driver nvs_flash esp_wifi esp_netif esp_http_server lwip json)