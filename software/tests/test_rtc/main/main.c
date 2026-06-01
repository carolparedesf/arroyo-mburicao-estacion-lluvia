#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rtc.h"

static const char *TAG = "TEST_RTC";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Test RTC DS3231 ===");

    ESP_ERROR_CHECK(ds3231_init());
    ESP_ERROR_CHECK(ds3231_enable_sqw_1hz());

    // Configurar fecha y hora actual
    ds3231_datetime_t ahora = {
        .segundos = 0,
        .minutos  = 0,
        .horas    = 12,
        .dia      = 5,
        .fecha    = 29,
        .mes      = 5,
        .anio     = 26,
    };
    ESP_ERROR_CHECK(ds3231_set_datetime(&ahora));
    ESP_LOGI(TAG, "Fecha/hora configurada correctamente");

    // Leer cada segundo durante 10 segundos
    ds3231_datetime_t dt;
    float temp;

    for (int i = 0; i < 10; i++) {
        if (ds3231_get_datetime(&dt) == ESP_OK) {
            ds3231_print_datetime(&dt);
        } else {
            ESP_LOGE(TAG, "Error leyendo fecha/hora");
        }

        if (ds3231_get_temperature(&temp) == ESP_OK) {
            ESP_LOGI(TAG, "Temperatura RTC: %.2f C", temp);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "=== Test RTC finalizado ===");
}