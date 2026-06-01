#pragma once

#include "esp_err.h"

esp_err_t sensor_lluvia_init(void);
esp_err_t sensor_lluvia_leer_acumulado(float *rain_mm);
esp_err_t sensor_lluvia_reset_acumulado(void);
