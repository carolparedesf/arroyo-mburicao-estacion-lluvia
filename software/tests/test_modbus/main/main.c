#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "modbus_rtu.h"

static const char *TAG = "TEST_MODBUS";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Test Modbus RTU - Sensor Lluvia ===");
    ESP_LOGI(TAG, "Slave ID: %d  Registro: 0x%04X  Baudrate: 9600",
             RAIN_SLAVE_ID, RAIN_REGISTER_ACCUMULATED, 9600);

    ESP_ERROR_CHECK(modbus_rtu_init());

    float rain_mm = 0.0f;
    int exitosas = 0;
    int fallidas  = 0;

    // 10 lecturas cada 2 segundos
    for (int i = 1; i <= 10; i++) {
        ESP_LOGI(TAG, "--- Lectura %d/10 ---", i);

        esp_err_t ret = modbus_read_rain(&rain_mm);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Lluvia acumulada: %.1f mm", rain_mm);
            exitosas++;
        } else {
            ESP_LOGW(TAG, "Error: %s", esp_err_to_name(ret));
            fallidas++;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Resumen
    ESP_LOGI(TAG, "──────────────────────────────────");
    ESP_LOGI(TAG, "Lecturas exitosas: %d/10", exitosas);
    ESP_LOGI(TAG, "Lecturas fallidas: %d/10", fallidas);

    // Reset del registro
    ESP_LOGI(TAG, "Reseteando registro acumulado...");
    if (modbus_reset_rain() == ESP_OK) {
        ESP_LOGI(TAG, "Reset exitoso");
        modbus_read_rain(&rain_mm);
        ESP_LOGI(TAG, "Valor tras reset: %.1f mm", rain_mm);
    } else {
        ESP_LOGW(TAG, "Error en reset");
    }

    ESP_LOGI(TAG, "=== Test Modbus finalizado ===");
}