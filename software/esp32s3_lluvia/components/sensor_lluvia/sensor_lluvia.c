#include "sensor_lluvia.h"

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "rs485.h"
#include "rs485_config.h"

static const char *TAG = "SENSOR_LLUVIA";

static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

esp_err_t sensor_lluvia_init(void)
{
    rs485_config_t cfg = {
        .uart_port = SENSOR_UART_PORT,
        .tx_pin = SENSOR_UART_TX,
        .rx_pin = SENSOR_UART_RX,
        .de_re_pin = SENSOR_UART_DE_RE,
        .baud_rate = SENSOR_BAUD_RATE,
    };

    esp_err_t err = rs485_init(&cfg);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Sensor lluvia RS485 inicializado. Slave ID=%d", RAIN_SLAVE_ID);
    } else {
        ESP_LOGE(TAG, "Error inicializando RS485 lluvia");
    }

    return err;
}

esp_err_t sensor_lluvia_leer_acumulado(float *rain_mm)
{
    if (rain_mm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t request[8];

    request[0] = RAIN_SLAVE_ID;
    request[1] = 0x03;
    request[2] = (RAIN_REGISTER_ACCUMULATED >> 8) & 0xFF;
    request[3] = RAIN_REGISTER_ACCUMULATED & 0xFF;
    request[4] = 0x00;
    request[5] = 0x01;

    uint16_t crc = modbus_crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    uint8_t response[RS485_BUF_SIZE] = {0};

    rs485_send(SENSOR_UART_PORT, SENSOR_UART_DE_RE, request, sizeof(request));

    int len = rs485_receive(
        SENSOR_UART_PORT,
        response,
        sizeof(response),
        pdMS_TO_TICKS(1000)
    );

    if (len < 7) {
        ESP_LOGW(TAG, "Respuesta Modbus incompleta. Len=%d", len);
        return ESP_FAIL;
    }

    uint16_t crc_rx = response[len - 2] | (response[len - 1] << 8);
    uint16_t crc_calc = modbus_crc16(response, len - 2);

    if (crc_rx != crc_calc) {
        ESP_LOGW(TAG, "CRC invalido. RX=0x%04X CALC=0x%04X", crc_rx, crc_calc);
        return ESP_FAIL;
    }

    if (response[0] != RAIN_SLAVE_ID || response[1] != 0x03 || response[2] != 0x02) {
        ESP_LOGW(TAG, "Respuesta Modbus inesperada");
        return ESP_FAIL;
    }

    uint16_t raw = ((uint16_t)response[3] << 8) | response[4];

    *rain_mm = raw / RAIN_SCALE_FACTOR_MM;

    ESP_LOGI(TAG, "Lluvia acumulada raw=%u -> %.2f mm", raw, *rain_mm);

    return ESP_OK;
}

esp_err_t sensor_lluvia_reset_acumulado(void)
{
    uint8_t request[8];

    request[0] = RAIN_SLAVE_ID;
    request[1] = 0x06;
    request[2] = (RAIN_REGISTER_ACCUMULATED >> 8) & 0xFF;
    request[3] = RAIN_REGISTER_ACCUMULATED & 0xFF;
    request[4] = 0x00;
    request[5] = 0x00;

    uint16_t crc = modbus_crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    uint8_t response[RS485_BUF_SIZE] = {0};

    rs485_send(SENSOR_UART_PORT, SENSOR_UART_DE_RE, request, sizeof(request));

    int len = rs485_receive(
        SENSOR_UART_PORT,
        response,
        sizeof(response),
        pdMS_TO_TICKS(1000)
    );

    if (len < 8) {
        ESP_LOGW(TAG, "Respuesta incompleta al reset. Len=%d", len);
        return ESP_FAIL;
    }

    uint16_t crc_rx = response[len - 2] | (response[len - 1] << 8);
    uint16_t crc_calc = modbus_crc16(response, len - 2);

    if (crc_rx != crc_calc) {
        ESP_LOGW(TAG, "CRC invalido en reset. RX=0x%04X CALC=0x%04X", crc_rx, crc_calc);
        return ESP_FAIL;
    }

    if (memcmp(request, response, 6) != 0) {
        ESP_LOGW(TAG, "Eco de reset Modbus inesperado");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Registro lluvia 0x%04X reiniciado correctamente", RAIN_REGISTER_ACCUMULATED);

    return ESP_OK;
}