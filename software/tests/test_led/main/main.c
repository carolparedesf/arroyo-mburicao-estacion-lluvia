#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TEST_LED";

#define LED_PIN GPIO_NUM_21

void app_main(void)
{
    ESP_LOGI(TAG, "=== Test LED Debug IO21 ===");

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    int estado = 0;
    while (1) {
        gpio_set_level(LED_PIN, estado);
        ESP_LOGI(TAG, "LED %s", estado ? "ENCENDIDO" : "APAGADO");
        estado = !estado;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}