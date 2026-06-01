#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t segundos;
    uint8_t minutos;
    uint8_t horas;
    uint8_t dia;
    uint8_t fecha;
    uint8_t mes;
    uint8_t anio;
} ds3231_datetime_t;

esp_err_t ds3231_init(void);
esp_err_t ds3231_set_datetime(const ds3231_datetime_t *dt);
esp_err_t ds3231_get_datetime(ds3231_datetime_t *dt);
esp_err_t ds3231_enable_sqw_1hz(void);
esp_err_t ds3231_get_temperature(float *temp_c);
void      ds3231_print_datetime(const ds3231_datetime_t *dt);