#pragma once

#include "driver/uart.h"
#include "driver/gpio.h"

/************************************************************
 * RS485 — Sensor de lluvia
 ************************************************************/

#define SENSOR_UART_PORT              UART_NUM_1
#define SENSOR_UART_TX                GPIO_NUM_1
#define SENSOR_UART_RX                GPIO_NUM_2
#define SENSOR_UART_DE_RE             GPIO_NUM_4

#define SENSOR_BAUD_RATE              9600
#define SENSOR_BUF_SIZE               256

// Modbus RTU — Sensor de lluvia
#define RAIN_SLAVE_ID                 1
#define RAIN_REGISTER_ACCUMULATED     0x006F
#define RAIN_SCALE_FACTOR_MM          10.0f

/************************************************************
 * RS485 — DTU / Modem
 ************************************************************/

#define DTU_UART_PORT                 UART_NUM_2
#define DTU_UART_TX                   GPIO_NUM_5
#define DTU_UART_RX                   GPIO_NUM_6
#define DTU_UART_DE_RE                GPIO_NUM_7

#define DTU_BAUD_RATE                 9600
#define DTU_BUF_SIZE                  256

/************************************************************
 * I2C — RTC DS3231M
 ************************************************************/

#define RTC_I2C_SDA                   GPIO_NUM_8
#define RTC_I2C_SCL                   GPIO_NUM_9

#define RTC_SQW_INT                   GPIO_NUM_16

/************************************************************
 * SPI — MicroSD
 ************************************************************/

#define SD_SPI_CS                     GPIO_NUM_10
#define SD_SPI_MOSI                   GPIO_NUM_11
#define SD_SPI_CLK                    GPIO_NUM_12
#define SD_SPI_MISO                   GPIO_NUM_13

#define SD_DETECT                     GPIO_NUM_15

/************************************************************
 * Debug
 ************************************************************/

#define LED_DBG                       GPIO_NUM_21