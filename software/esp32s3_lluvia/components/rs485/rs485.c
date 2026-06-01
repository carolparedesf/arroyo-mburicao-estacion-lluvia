#include "rs485.h"
#include "esp_log.h"

static const char *TAG = "RS485";

esp_err_t rs485_init(const rs485_config_t *cfg)
{
    // Configuración del UART
    uart_config_t uart_cfg = {
        .baud_rate  = cfg->baud_rate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret;

    ret = uart_param_config(cfg->uart_port, &uart_cfg);
    if (ret != ESP_OK) return ret;

    ret = uart_set_pin(cfg->uart_port,
                       cfg->tx_pin,
                       cfg->rx_pin,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;

    ret = uart_driver_install(cfg->uart_port,
                              RS485_BUF_SIZE * 2,  // RX buffer
                              RS485_BUF_SIZE * 2,  // TX buffer
                              0, NULL, 0);
    if (ret != ESP_OK) return ret;

    // Modo RS485 half-duplex con control manual de DE/RE
    ret = uart_set_mode(cfg->uart_port, UART_MODE_RS485_HALF_DUPLEX);
    if (ret != ESP_OK) return ret;

    // Configurar pin DE/RE como salida
    gpio_reset_pin(cfg->de_re_pin);
    gpio_set_direction(cfg->de_re_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(cfg->de_re_pin, 0);  // modo recepción por defecto

    ESP_LOGI(TAG, "UART%d iniciado — TX:%d RX:%d DE/RE:%d @ %d baud",
             cfg->uart_port, cfg->tx_pin, cfg->rx_pin,
             cfg->de_re_pin, cfg->baud_rate);

    return ESP_OK;
}


int rs485_send(uart_port_t port, gpio_num_t de_re, const uint8_t *data, size_t len)
{
    gpio_set_level(de_re, 1);
    vTaskDelay(pdMS_TO_TICKS(2));

    int sent = uart_write_bytes(port, (const char *)data, len);
    uart_wait_tx_done(port, pdMS_TO_TICKS(100));

    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(de_re, 0);

    return sent;
}

int rs485_receive(uart_port_t port, uint8_t *buf, size_t max_len, TickType_t timeout)
{
    return uart_read_bytes(port, buf, max_len, timeout);
}