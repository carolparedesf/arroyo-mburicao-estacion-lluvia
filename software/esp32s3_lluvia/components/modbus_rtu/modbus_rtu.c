#include "modbus_rtu.h"
#include "rs485_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MODBUS";

#define MODBUS_TIMEOUT_MS    500

// ── CRC16 Modbus ──────────────────────────────────────────────
static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else              crc >>= 1;
        }
    }
    return crc;
}

// ── Init UART1 para sensor ────────────────────────────────────
esp_err_t modbus_rtu_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = SENSOR_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(SENSOR_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SENSOR_UART_PORT,
                                  SENSOR_UART_TX,
                                  SENSOR_UART_RX,
                                  UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(SENSOR_UART_PORT,
                                         SENSOR_BUF_SIZE * 2,
                                         SENSOR_BUF_SIZE * 2,
                                         0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(SENSOR_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    gpio_reset_pin(SENSOR_UART_DE_RE);
    gpio_set_direction(SENSOR_UART_DE_RE, GPIO_MODE_OUTPUT);
    gpio_set_level(SENSOR_UART_DE_RE, 0);

    ESP_LOGI(TAG, "Modbus RTU inicializado — UART%d @ %d baud, slave ID %d",
             SENSOR_UART_PORT, SENSOR_BAUD_RATE, RAIN_SLAVE_ID);
    return ESP_OK;
}

// ── Enviar trama ──────────────────────────────────────────────
static void modbus_send(const uint8_t *frame, size_t len)
{
    gpio_set_level(SENSOR_UART_DE_RE, 1);
    uart_write_bytes(SENSOR_UART_PORT, frame, len);
    uart_wait_tx_done(SENSOR_UART_PORT, pdMS_TO_TICKS(100));
    gpio_set_level(SENSOR_UART_DE_RE, 0);
}

// ── Recibir respuesta ─────────────────────────────────────────
static int modbus_receive(uint8_t *buf, size_t max_len)
{
    return uart_read_bytes(SENSOR_UART_PORT, buf,
                           max_len, pdMS_TO_TICKS(MODBUS_TIMEOUT_MS));
}

// ── FC03: Leer registro holding ───────────────────────────────
static esp_err_t modbus_read_register(uint16_t reg_addr, uint16_t *value)
{
    uint8_t frame[8] = {
        RAIN_SLAVE_ID,
        0x03,
        (reg_addr >> 8) & 0xFF,
        reg_addr & 0xFF,
        0x00, 0x01,
        0x00, 0x00,
    };
    uint16_t crc = crc16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    uart_flush_input(SENSOR_UART_PORT);
    modbus_send(frame, sizeof(frame));

    uint8_t response[8] = {0};
    int received = modbus_receive(response, sizeof(response));

    if (received < 5) {
        ESP_LOGW(TAG, "Respuesta insuficiente: %d bytes", received);
        return ESP_ERR_TIMEOUT;
    }

    uint16_t crc_calc = crc16(response, received - 2);
    uint16_t crc_recv = response[received-2] | (response[received-1] << 8);
    if (crc_calc != crc_recv) {
        ESP_LOGW(TAG, "CRC incorrecto: calc=0x%04X recv=0x%04X", crc_calc, crc_recv);
        return ESP_ERR_INVALID_CRC;
    }

    if (response[0] != RAIN_SLAVE_ID || response[1] != 0x03) {
        ESP_LOGW(TAG, "Respuesta inesperada: ID=%d FC=%d", response[0], response[1]);
        return ESP_FAIL;
    }

    *value = (response[3] << 8) | response[4];
    return ESP_OK;
}

// ── FC06: Escribir registro holding ───────────────────────────
static esp_err_t modbus_write_register(uint16_t reg_addr, uint16_t value)
{
    uint8_t frame[8] = {
        RAIN_SLAVE_ID,
        0x06,
        (reg_addr >> 8) & 0xFF,
        reg_addr & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF,
        0x00, 0x00,
    };
    uint16_t crc = crc16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    uart_flush_input(SENSOR_UART_PORT);
    modbus_send(frame, sizeof(frame));

    uint8_t response[8] = {0};
    int received = modbus_receive(response, sizeof(response));

    if (received < 6) {
        ESP_LOGW(TAG, "Sin confirmacion de escritura: %d bytes", received);
        return ESP_ERR_TIMEOUT;
    }

    uint16_t crc_calc = crc16(response, received - 2);
    uint16_t crc_recv = response[received-2] | (response[received-1] << 8);
    if (crc_calc != crc_recv) {
        ESP_LOGW(TAG, "CRC escritura incorrecto");
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

// ── API pública ───────────────────────────────────────────────
esp_err_t modbus_read_rain(float *rain_mm)
{
    uint16_t raw = 0;
    esp_err_t ret = modbus_read_register(RAIN_REGISTER_ACCUMULATED, &raw);
    if (ret != ESP_OK) return ret;

    *rain_mm = raw / RAIN_SCALE_FACTOR_MM;
    ESP_LOGI(TAG, "Lluvia acumulada: raw=%d → %.1f mm", raw, *rain_mm);
    return ESP_OK;
}

esp_err_t modbus_reset_rain(void)
{
    esp_err_t ret = modbus_write_register(RAIN_REGISTER_ACCUMULATED, 0x0000);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Registro de lluvia reseteado");
    }
    return ret;
}