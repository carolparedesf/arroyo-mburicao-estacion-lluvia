#pragma once

#include "esp_err.h"
#include <stdbool.h>

// Inicializa SPI y monta la SD
esp_err_t sdcard_init(void);

// Verifica si hay tarjeta insertada
bool sdcard_is_inserted(void);

// Crea archivo con cabecera si no existe
esp_err_t sdcard_ensure_file(const char *path, const char *header);

// Agrega una línea al final de un archivo
esp_err_t sdcard_append_line(const char *path, const char *line);

// Lee la primera línea de datos (saltando cabecera)
esp_err_t sdcard_read_first_record(const char *path, char *buf, size_t buf_len);

// Elimina el primer registro (reescribe sin él)
esp_err_t sdcard_remove_first_record(const char *path, const char *header);

// Cuenta registros (sin cabecera)
uint32_t sdcard_count_records(const char *path);