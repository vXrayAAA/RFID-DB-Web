#ifndef RC522_H
#define RC522_H

#include "driver/spi_master.h"
#include "driver/gpio.h"

// #define RC522_SPI_BUS_GPIO_MISO    (37)
// #define RC522_SPI_BUS_GPIO_MOSI    (35)
// #define RC522_SPI_BUS_GPIO_SCLK    (36)
// #define RC522_SPI_SCANNER_GPIO_SDA (39)
// #define RC522_SCANNER_GPIO_RST     (-1) // soft-reset


// Pinos para ESP32-S3
#define RC522_SPI_HOST          SPI2_HOST
#define RC522_PIN_MISO          37
#define RC522_PIN_MOSI          35
#define RC522_PIN_CLK           36
#define RC522_PIN_CS            39
#define RC522_PIN_RST           -1

// Comandos RC522
#define RC522_CMD_IDLE          0x00
#define RC522_CMD_MEM           0x01
#define RC522_CMD_RANDOM_ID     0x02
#define RC522_CMD_CALC_CRC      0x03
#define RC522_CMD_TRANSMIT      0x04
#define RC522_CMD_NO_CMD_CHANGE 0x07
#define RC522_CMD_RECEIVE       0x08
#define RC522_CMD_TRANSCEIVE    0x0C
#define RC522_CMD_AUTH          0x0E
#define RC522_CMD_SOFT_RESET    0x0F

// Registradores RC522
#define RC522_REG_COMMAND       0x01
#define RC522_REG_COMM_IE       0x02
#define RC522_REG_DIV1_EN       0x03
#define RC522_REG_COMM_IRQ      0x04
#define RC522_REG_DIV_IRQ       0x05
#define RC522_REG_ERROR         0x06
#define RC522_REG_STATUS1       0x07
#define RC522_REG_STATUS2       0x08
#define RC522_REG_FIFO_DATA     0x09
#define RC522_REG_FIFO_LEVEL    0x0A
#define RC522_REG_WATER_LEVEL   0x0B
#define RC522_REG_CONTROL       0x0C
#define RC522_REG_BIT_FRAMING   0x0D
#define RC522_REG_COLL          0x0E
#define RC522_REG_MODE          0x11
#define RC522_REG_TX_MODE       0x12
#define RC522_REG_RX_MODE       0x13
#define RC522_REG_TX_CONTROL    0x14
#define RC522_REG_TX_AUTO       0x15
#define RC522_REG_TX_SEL        0x16
#define RC522_REG_RX_SEL        0x17
#define RC522_REG_RX_THRESHOLD  0x18
#define RC522_REG_DEMOD         0x19
#define RC522_REG_MF_TX         0x1C
#define RC522_REG_MF_RX         0x1D
#define RC522_REG_SERIAL_SPEED  0x1F
#define RC522_REG_CRC_RESULT_M  0x21
#define RC522_REG_CRC_RESULT_L  0x22
#define RC522_REG_MOD_WIDTH     0x24
#define RC522_REG_RF_CFG        0x26
#define RC522_REG_GS_N          0x27
#define RC522_REG_CW_GS_P       0x28
#define RC522_REG_MOD_GS_P      0x29
#define RC522_REG_T_MODE        0x2A
#define RC522_REG_T_PRESCALER   0x2B
#define RC522_REG_T_RELOAD_H    0x2C
#define RC522_REG_T_RELOAD_L    0x2D
#define RC522_REG_T_COUNTER_VAL_H 0x2E
#define RC522_REG_T_COUNTER_VAL_L 0x2F

// Códigos de status
#define RC522_OK                0
#define RC522_ERR_TIMEOUT      -1
#define RC522_ERR_NO_CARD      -2
#define RC522_ERR_CRC          -3
#define RC522_ERR_COLLISION    -4

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t sak;
} rc522_card_t;

typedef struct {
    spi_device_handle_t spi_handle;
    bool initialized;
} rc522_handle_t;

// Funções públicas
esp_err_t rc522_init(rc522_handle_t *handle);
esp_err_t rc522_deinit(rc522_handle_t *handle);
esp_err_t rc522_test_communication(rc522_handle_t *handle);
int rc522_card_present(rc522_handle_t *handle);
int rc522_read_card(rc522_handle_t *handle, rc522_card_t *card);
esp_err_t rc522_read_card_uid(rc522_handle_t *handle, char *uid_str, size_t uid_str_size);
void rc522_antenna_on(rc522_handle_t *handle);
void rc522_antenna_off(rc522_handle_t *handle);

#endif // RC522_H
