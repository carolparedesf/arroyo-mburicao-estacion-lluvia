#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Inicializa Modbus RTU en el canal RS485 del sensor
esp_err_t modbus_rtu_init(void);

// Lee lluvia acumulada del registro 0x006F
// Retorna el valor en mm (raw / 10.0)
esp_err_t modbus_read_rain(float *rain_mm);

// Resetea el registro de lluvia acumulada escribiendo 0
esp_err_t modbus_reset_rain(void);