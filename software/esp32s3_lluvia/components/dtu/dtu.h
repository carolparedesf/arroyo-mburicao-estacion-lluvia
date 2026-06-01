#pragma once

#include "esp_err.h"
#include <stdint.h>

// Inicializa la comunicación con el DTU
esp_err_t dtu_init(void);

// Envía un string al DTU y espera respuesta
// Retorna los bytes recibidos, 0 si no hubo respuesta
int dtu_send_and_receive(const char *data, char *response_buf, size_t buf_len);