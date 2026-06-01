#pragma once

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define RS485_BUF_SIZE 256

typedef struct {
    uart_port_t uart_port;
    gpio_num_t  tx_pin;
    gpio_num_t  rx_pin;
    gpio_num_t  de_re_pin;
    int         baud_rate;
} rs485_config_t;

// Inicializa un bus RS485
esp_err_t rs485_init(const rs485_config_t *cfg);

// Envía datos (activa DE/RE automáticamente)
int rs485_send(uart_port_t port, gpio_num_t de_re, const uint8_t *data, size_t len);

// Recibe datos
int rs485_receive(uart_port_t port, uint8_t *buf, size_t max_len, TickType_t timeout);
