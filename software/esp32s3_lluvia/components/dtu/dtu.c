#include "dtu.h"
#include "rs485_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "DTU";

esp_err_t dtu_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = 115200,          // mismo que tu código Arduino
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(DTU_UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(DTU_UART_PORT,
                                  DTU_UART_TX,
                                  DTU_UART_RX,
                                  UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(DTU_UART_PORT,
                                         
                                         DTU_BUF_SIZE * 2,
                                         DTU_BUF_SIZE * 2,
                                         0, NULL, 0));
    ESP_ERROR_CHECK(uart_set_mode(DTU_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    // Pin DE/RE como salida, modo recepción por defecto
    gpio_reset_pin(DTU_UART_DE_RE);
    gpio_set_direction(DTU_UART_DE_RE, GPIO_MODE_OUTPUT);
    gpio_set_level(DTU_UART_DE_RE, 0);

    ESP_LOGI(TAG, "DTU inicializado en UART%d @ 115200", DTU_UART_PORT);

    // Esperar que el modem arranque (equivalente al delay(5000) de Arduino)
    ESP_LOGI(TAG, "Esperando que el modem arranque...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    return ESP_OK;
}

int dtu_send_and_receive(const char *data, char *response_buf, size_t buf_len)
{
    size_t len = strlen(data);

    // Activar TX
    gpio_set_level(DTU_UART_DE_RE, 1);
    uart_write_bytes(DTU_UART_PORT, data, len);
    uart_wait_tx_done(DTU_UART_PORT, pdMS_TO_TICKS(100));
    gpio_set_level(DTU_UART_DE_RE, 0);  // volver a RX

    ESP_LOGI(TAG, "Enviado: %s", data);

    // Esperar respuesta (equivalente al delay(2000) de Arduino)
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Leer respuesta
    int received = uart_read_bytes(DTU_UART_PORT,
                                   (uint8_t *)response_buf,
                                   buf_len - 1,
                                   pdMS_TO_TICKS(500));
    if (received > 0) {
        response_buf[received] = '\0';
        ESP_LOGI(TAG, "Respuesta: %s", response_buf);
    } else {
        ESP_LOGW(TAG, "Sin respuesta del modem");
    }

    return received;
}