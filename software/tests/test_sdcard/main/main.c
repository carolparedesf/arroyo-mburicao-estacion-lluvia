#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdcard.h"

static const char *TAG = "TEST_SDCARD";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Test MicroSD ===");

    // 1. Inicializar
    esp_err_t ret = sdcard_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando SD: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SD inicializada correctamente");

    // 2. Verificar deteccion
    ESP_LOGI(TAG, "Tarjeta insertada: %s", sdcard_is_inserted() ? "SI" : "NO");

    // 3. Crear archivos con cabecera
    sdcard_ensure_file("/test_log.csv",   "Timestamp,Valor");
    sdcard_ensure_file("/test_queue.csv", "Timestamp,Valor");
    ESP_LOGI(TAG, "Archivos creados");

    // 4. Escribir registros de prueba
    sdcard_append_line("/test_log.csv",   "2026-05-29 12:00:00,1.5");
    sdcard_append_line("/test_log.csv",   "2026-05-29 12:01:00,2.3");
    sdcard_append_line("/test_log.csv",   "2026-05-29 12:02:00,0.8");
    sdcard_append_line("/test_queue.csv", "2026-05-29 12:00:00,1.5");
    ESP_LOGI(TAG, "Registros escritos");

    // 5. Contar registros
    ESP_LOGI(TAG, "Registros en test_log.csv:   %lu", sdcard_count_records("/test_log.csv"));
    ESP_LOGI(TAG, "Registros en test_queue.csv: %lu", sdcard_count_records("/test_queue.csv"));

    // 6. Leer primer registro
    char buf[128] = {0};
    if (sdcard_read_first_record("/test_queue.csv", buf, sizeof(buf)) == ESP_OK) {
        ESP_LOGI(TAG, "Primer registro en cola: %s", buf);
    }

    // 7. Eliminar primer registro
    sdcard_remove_first_record("/test_queue.csv", "Timestamp,Valor");
    ESP_LOGI(TAG, "Registro eliminado");
    ESP_LOGI(TAG, "Registros en test_queue.csv tras eliminar: %lu",
             sdcard_count_records("/test_queue.csv"));

    ESP_LOGI(TAG, "=== Test SD finalizado ===");
}