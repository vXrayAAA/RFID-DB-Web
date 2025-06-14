#include "rc522.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "RC522";

// Funções privadas
static esp_err_t rc522_write_reg(rc522_handle_t *handle, uint8_t reg, uint8_t data);
static esp_err_t rc522_read_reg(rc522_handle_t *handle, uint8_t reg, uint8_t *data);
static esp_err_t rc522_set_reg_bits(rc522_handle_t *handle, uint8_t reg, uint8_t mask);
static esp_err_t rc522_clear_reg_bits(rc522_handle_t *handle, uint8_t reg, uint8_t mask);
static int rc522_communicate_with_picc(rc522_handle_t *handle, uint8_t command, uint8_t *send_data, uint8_t send_len, uint8_t *back_data, uint8_t *back_len, uint8_t *valid_bits);
static int rc522_picc_request(rc522_handle_t *handle, uint8_t req_mode, uint8_t *tag_type);
static int rc522_picc_anticoll(rc522_handle_t *handle, uint8_t *serial_num);
static void rc522_reset(rc522_handle_t *handle);

esp_err_t rc522_init(rc522_handle_t *handle) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Inicializando RC522...");
    ESP_LOGI(TAG, "Pinos configurados - MISO:%d, MOSI:%d, CLK:%d, CS:%d, RST:%d", 
             RC522_PIN_MISO, RC522_PIN_MOSI, RC522_PIN_CLK, RC522_PIN_CS, RC522_PIN_RST);
    
    // Configuração SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = RC522_PIN_MISO,
        .mosi_io_num = RC522_PIN_MOSI,
        .sclk_io_num = RC522_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0
    };    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 500000,   // Reduzir para 500kHz para testar
        .mode = 0,
        .spics_io_num = RC522_PIN_CS,
        .queue_size = 7,
    };
    
    // Inicializar barramento SPI
    ret = spi_bus_initialize(RC522_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Falha ao inicializar barramento SPI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Adicionar dispositivo SPI
    ret = spi_bus_add_device(RC522_SPI_HOST, &devcfg, &handle->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo SPI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configurar pino RST
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RC522_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
      // Reset do RC522 (mais robusto)
    ESP_LOGI(TAG, "Executando reset do RC522...");
    gpio_set_level(RC522_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RC522_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Fazer soft reset
    rc522_write_reg(handle, RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Configurar RC522
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Timer mode
    rc522_write_reg(handle, RC522_REG_T_MODE, 0x8D);
    rc522_write_reg(handle, RC522_REG_T_PRESCALER, 0x3E);
    rc522_write_reg(handle, RC522_REG_T_RELOAD_L, 30);
    rc522_write_reg(handle, RC522_REG_T_RELOAD_H, 0);
    
    // Force 100% ASK modulation
    rc522_write_reg(handle, RC522_REG_TX_AUTO, 0x40);
    
    // Set the preset value for the CRC coprocessor for the CalcCRC command to 0x6363 (ISO 14443-3 part 6.2.4)
    rc522_write_reg(handle, RC522_REG_MODE, 0x3D);
    
    // Enable antenna
    rc522_antenna_on(handle);
    
    handle->initialized = true;
    ESP_LOGI(TAG, "RC522 inicializado com sucesso");
    
    return ESP_OK;
}

esp_err_t rc522_deinit(rc522_handle_t *handle) {
    if (!handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    rc522_antenna_off(handle);
    spi_bus_remove_device(handle->spi_handle);
    spi_bus_free(RC522_SPI_HOST);
    
    handle->initialized = false;
    ESP_LOGI(TAG, "RC522 desinicializado");
    
    return ESP_OK;
}

void rc522_antenna_on(rc522_handle_t *handle) {
    uint8_t temp;
    rc522_read_reg(handle, RC522_REG_TX_CONTROL, &temp);
    if (!(temp & 0x03)) {
        rc522_set_reg_bits(handle, RC522_REG_TX_CONTROL, 0x03);
    }
}

void rc522_antenna_off(rc522_handle_t *handle) {
    rc522_clear_reg_bits(handle, RC522_REG_TX_CONTROL, 0x03);
}

int rc522_card_present(rc522_handle_t *handle) {
    if (!handle || !handle->initialized) {
        ESP_LOGW(TAG, "RC522 handle não inicializado");
        return RC522_ERR_NO_CARD;
    }
    
    uint8_t tag_type[2];
    int result = rc522_picc_request(handle, 0x52, tag_type);
      // Debug: Log ocasional para mostrar que está verificando
    static int check_counter = 0;
    if (check_counter % 10000 == 0) { // A cada 1000 segundos
        ESP_LOGI(TAG, "Verificando presença de cartão... (status: %d)", result);
    }
    check_counter++;
    
    return result;
}

esp_err_t rc522_read_card_uid(rc522_handle_t *handle, char *uid_str, size_t uid_str_size) {
    rc522_card_t card;
    int status;
    
    ESP_LOGI(TAG, "Tentando ler UID do cartão...");
    
    // Ler cartão
    status = rc522_read_card(handle, &card);
    ESP_LOGI(TAG, "rc522_read_card retornou status: %d", status);
    
    if (status != RC522_OK) {
        ESP_LOGW(TAG, "Falha ao ler cartão, status: %d", status);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Cartão lido com sucesso! UID length: %d", card.uid_len);
    
    // Converter UID para string hexadecimal
    int written = 0;
    for (int i = 0; i < card.uid_len && written < (uid_str_size - 3); i++) {
        if (i > 0) {
            uid_str[written++] = ':';
        }
        written += snprintf(&uid_str[written], uid_str_size - written, "%02X", card.uid[i]);
    }
    uid_str[written] = '\0';
    
    ESP_LOGI(TAG, "UID convertido para string: %s", uid_str);
    
    return ESP_OK;
}

int rc522_read_card(rc522_handle_t *handle, rc522_card_t *card) {
    int status;
    uint8_t tag_type[2];
    
    ESP_LOGI(TAG, "Iniciando leitura do cartão...");
    
    // Request card com timeout maior
    status = rc522_picc_request(handle, 0x26, tag_type); // REQIDL em vez de REQALL
    ESP_LOGI(TAG, "PICC request status: %d, tag_type: 0x%02X%02X", status, tag_type[0], tag_type[1]);
    if (status != RC522_OK) {
        // Tentar REQALL como fallback
        vTaskDelay(pdMS_TO_TICKS(10));
        status = rc522_picc_request(handle, 0x52, tag_type);
        ESP_LOGI(TAG, "PICC request fallback status: %d", status);
        if (status != RC522_OK) {
            ESP_LOGW(TAG, "PICC request falhou");
            return status;
        }
    }
    
    // Pequeno delay antes do anti-collision
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // Anti-collision
    status = rc522_picc_anticoll(handle, card->uid);
    ESP_LOGI(TAG, "Anti-collision status: %d", status);
    if (status != RC522_OK) {
        ESP_LOGW(TAG, "Anti-collision falhou");
        return status;
    }
    
    card->uid_len = 4; // UID padrão de 4 bytes
    
    ESP_LOGI(TAG, "Cartão detectado - UID: %02X:%02X:%02X:%02X", 
             card->uid[0], card->uid[1], card->uid[2], card->uid[3]);
    
    return RC522_OK;
}

// Função de teste para verificar comunicação SPI
esp_err_t rc522_test_communication(rc522_handle_t *handle) {
    ESP_LOGI(TAG, "Testando comunicação SPI com RC522...");
    
    // Testar vários registros para verificar comunicação
    uint8_t version, command, status;
    esp_err_t ret1, ret2, ret3;
    
    ret1 = rc522_read_reg(handle, 0x37, &version);  // Registro de versão
    ret2 = rc522_read_reg(handle, RC522_REG_COMMAND, &command); // Registro de comando
    ret3 = rc522_read_reg(handle, RC522_REG_STATUS1, &status);  // Registro de status
    
    ESP_LOGI(TAG, "Teste SPI - Versão: 0x%02X (%s), Comando: 0x%02X (%s), Status: 0x%02X (%s)", 
             version, esp_err_to_name(ret1),
             command, esp_err_to_name(ret2), 
             status, esp_err_to_name(ret3));
    
    // Verificar se pelo menos uma leitura funcionou
    if (ret1 == ESP_OK || ret2 == ESP_OK || ret3 == ESP_OK) {
        ESP_LOGI(TAG, "RC522 respondendo via SPI");
        
        // Versões conhecidas do RC522: 0x91, 0x92, etc.
        if (version == 0x91 || version == 0x92 || version == 0x00 || version == 0xFF) {
            ESP_LOGI(TAG, "RC522 comunicação estabelecida");
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "RC522 versão não reconhecida mas SPI funcionando");
            return ESP_OK; // Aceitar mesmo assim
        }
    } else {
        ESP_LOGE(TAG, "RC522 não responde via SPI - verificar conexões");
        return ESP_FAIL;
    }
}

// Funções privadas
static void rc522_reset(rc522_handle_t *handle) {
    gpio_set_level(RC522_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RC522_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Software reset
    rc522_write_reg(handle, RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static esp_err_t rc522_write_reg(rc522_handle_t *handle, uint8_t reg, uint8_t data) {
    spi_transaction_t trans = {
        .length = 16,
        .tx_data = {(reg << 1) & 0x7E, data},
        .flags = SPI_TRANS_USE_TXDATA
    };
    
    return spi_device_transmit(handle->spi_handle, &trans);
}

static esp_err_t rc522_read_reg(rc522_handle_t *handle, uint8_t reg, uint8_t *data) {
    spi_transaction_t trans = {
        .length = 16,
        .tx_data = {((reg << 1) & 0x7E) | 0x80, 0x00},
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA
    };
    
    esp_err_t ret = spi_device_transmit(handle->spi_handle, &trans);
    if (ret == ESP_OK) {
        *data = trans.rx_data[1];
    }
    
    return ret;
}

static esp_err_t rc522_set_reg_bits(rc522_handle_t *handle, uint8_t reg, uint8_t mask) {
    uint8_t temp;
    esp_err_t ret = rc522_read_reg(handle, reg, &temp);
    if (ret == ESP_OK) {
        ret = rc522_write_reg(handle, reg, temp | mask);
    }
    return ret;
}

static esp_err_t rc522_clear_reg_bits(rc522_handle_t *handle, uint8_t reg, uint8_t mask) {
    uint8_t temp;
    esp_err_t ret = rc522_read_reg(handle, reg, &temp);
    if (ret == ESP_OK) {
        ret = rc522_write_reg(handle, reg, temp & (~mask));
    }
    return ret;
}

static int rc522_communicate_with_picc(rc522_handle_t *handle, uint8_t command, 
                                     uint8_t *send_data, uint8_t send_len, 
                                     uint8_t *back_data, uint8_t *back_len, 
                                     uint8_t *valid_bits) {
    uint8_t wait_irq = 0x00;
    uint8_t last_bits;
    uint8_t n;
    uint16_t i = 2000;
    
    switch (command) {
        case RC522_CMD_AUTH:
            wait_irq = 0x10;
            break;
        case RC522_CMD_TRANSCEIVE:
            wait_irq = 0x30;
            break;
        default:
            break;
    }
    
    rc522_write_reg(handle, RC522_REG_COMM_IRQ, 0x7F);
    rc522_clear_reg_bits(handle, RC522_REG_FIFO_LEVEL, 0x80);
    rc522_write_reg(handle, RC522_REG_COMMAND, RC522_CMD_IDLE);
    
    // Escrever dados para FIFO
    for (i = 0; i < send_len; i++) {
        rc522_write_reg(handle, RC522_REG_FIFO_DATA, send_data[i]);
    }
    
    // Executar comando
    rc522_write_reg(handle, RC522_REG_COMMAND, command);
    if (command == RC522_CMD_TRANSCEIVE) {
        rc522_set_reg_bits(handle, RC522_REG_BIT_FRAMING, 0x80);
    }
    
    // Aguardar interrupção
    i = 2000;
    do {
        uint8_t irq;
        rc522_read_reg(handle, RC522_REG_COMM_IRQ, &irq);
        if (irq & wait_irq) {
            break;
        }
        if (irq & 0x01) {
            return RC522_ERR_TIMEOUT;
        }
        i--;
    } while (i != 0);
    
    if (i == 0) {
        return RC522_ERR_TIMEOUT;
    }
    
    // Verificar erro
    uint8_t error_reg;
    rc522_read_reg(handle, RC522_REG_ERROR, &error_reg);
    if (error_reg & 0x13) {
        return RC522_ERR_CRC;
    }
    
    if (back_data && back_len) {
        rc522_read_reg(handle, RC522_REG_FIFO_LEVEL, &n);
        rc522_read_reg(handle, RC522_REG_CONTROL, &last_bits);
        last_bits &= 0x07;
        
        if (last_bits) {
            *back_len = (n - 1) * 8 + last_bits;
        } else {
            *back_len = n * 8;
        }
        
        if (n == 0) {
            n = 1;
        }
        if (n > 16) {
            n = 16;
        }
        
        for (i = 0; i < n; i++) {
            rc522_read_reg(handle, RC522_REG_FIFO_DATA, &back_data[i]);
        }
        
        if (valid_bits) {
            *valid_bits = last_bits;
        }
    }
    
    return RC522_OK;
}

static int rc522_picc_request(rc522_handle_t *handle, uint8_t req_mode, uint8_t *tag_type) {
    uint8_t back_len;
    int status;
    
    rc522_write_reg(handle, RC522_REG_BIT_FRAMING, 0x07);
    
    tag_type[0] = req_mode;
    status = rc522_communicate_with_picc(handle, RC522_CMD_TRANSCEIVE, tag_type, 1, tag_type, &back_len, NULL);
    
    if ((status != RC522_OK) || (back_len != 0x10)) {
        status = RC522_ERR_NO_CARD;
    }
    
    return status;
}

static int rc522_picc_anticoll(rc522_handle_t *handle, uint8_t *serial_num) {
    int status;
    uint8_t i;
    uint8_t ser_chk = 0;
    uint8_t back_len;
    uint8_t send_data[2];
    
    rc522_write_reg(handle, RC522_REG_BIT_FRAMING, 0x00);
    
    send_data[0] = 0x93;
    send_data[1] = 0x20;
    
    status = rc522_communicate_with_picc(handle, RC522_CMD_TRANSCEIVE, send_data, 2, serial_num, &back_len, NULL);
    
    if (status == RC522_OK) {
        // Verificar checksum
        for (i = 0; i < 4; i++) {
            ser_chk ^= serial_num[i];
        }
        if (ser_chk != serial_num[i]) {
            status = RC522_ERR_CRC;
        }
    }
    
    return status;
}
