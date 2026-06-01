#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rtc.h"
#include "sdcard.h"
#include "modbus_rtu.h"
#include "dtu.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MAIN";

#define MEASUREMENT_PERIOD_SEC      60
#define MAX_EXECUTION_LAG_SEC       2
#define QUEUE_RETRY_INTERVAL_MS     10000
#define LOOP_IDLE_MS                50

#define MAIN_LOG_FILE   "/datalog.csv"
#define TEMP_QUEUE_FILE "/temp.csv"
#define CSV_HEADER      "Timestamp,Lluvia_mm"

#define SERVER_TIME_OFFSET_SEC 10800

static uint32_t last_slot_epoch  = 0;
static uint32_t last_queue_retry = 0;

static void format_timestamp(const ds3231_datetime_t *dt, char *buf, size_t len)
{
    snprintf(buf, len, "20%02d-%02d-%02d %02d:%02d:%02d",
             dt->anio, dt->mes, dt->fecha,
             dt->horas, dt->minutos, dt->segundos);
}

static uint32_t rtc_to_epoch(const ds3231_datetime_t *dt)
{
    uint32_t days = dt->anio * 365 + dt->mes * 30 + dt->fecha;
    return days * 86400 + dt->horas * 3600 + dt->minutos * 60 + dt->segundos;
}

static bool is_measurement_due(const ds3231_datetime_t *dt, uint32_t *slot_out)
{
    uint32_t epoch = rtc_to_epoch(dt);
    uint32_t slot  = (epoch / MEASUREMENT_PERIOD_SEC) * MEASUREMENT_PERIOD_SEC;
    uint32_t delta = epoch - slot;

    if (delta > MAX_EXECUTION_LAG_SEC) return false;
    if (slot == last_slot_epoch)       return false;

    *slot_out = slot;
    return true;
}

static void build_csv_line(const char *timestamp, float rain_mm, char *buf, size_t len)
{
    snprintf(buf, len, "%s,%.1f", timestamp, rain_mm);
}

static uint32_t datetime_to_unix(const ds3231_datetime_t *dt)
{
    // Días desde epoch hasta 2000-01-01 = 10957
    uint16_t year = 2000 + dt->anio;
    uint16_t month = dt->mes;
    uint16_t day   = dt->fecha;

    // Algoritmo de días desde epoch
    if (month < 3) { year--; month += 12; }
    uint32_t days = 365 * year + year/4 - year/100 + year/400
                  + (153 * month - 457) / 5
                  + day - 719469;  // ajuste a epoch 1970

    return days * 86400
         + dt->horas   * 3600
         + dt->minutos * 60
         + dt->segundos;
}

static bool send_to_server(const char *timestamp, float rain_mm)
{
    ds3231_datetime_t dt = {0};

    sscanf(timestamp, "20%hhu-%hhu-%hhu %hhu:%hhu:%hhu",
           &dt.anio, &dt.mes, &dt.fecha,
           &dt.horas, &dt.minutos, &dt.segundos);

   
    uint32_t unix_ts = datetime_to_unix(&dt) + SERVER_TIME_OFFSET_SEC;

    char payload[64];
    snprintf(payload, sizeof(payload), "d=%lu,%.1f", unix_ts, rain_mm);

    char response[256] = {0};
    int bytes = dtu_send_and_receive(payload, response, sizeof(response));

    if (bytes <= 0) {
        ESP_LOGW(TAG, "Sin respuesta del DTU");
        return false;
    }

    if (strstr(response, "ERROR") || strstr(response, "FAIL")) {
        ESP_LOGW(TAG, "DTU respondio error: %s", response);
        return false;
    }

    ESP_LOGI(TAG, "DTU respuesta: %s", response);
    return true;
}

static void tomar_medicion(uint32_t slot)
{
    ESP_LOGI(TAG, "──────────────────────────────────");
    ESP_LOGI(TAG, "Iniciando medicion. Slot=%lu", slot);

    float rain_mm = 0.0f;
    esp_err_t ret = modbus_read_rain(&rain_mm);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fallo lectura Modbus. Ciclo cancelado.");
        return;
    }

    ds3231_datetime_t dt;
    char timestamp[24] = {0};
    if (ds3231_get_datetime(&dt) == ESP_OK) {
        format_timestamp(&dt, timestamp, sizeof(timestamp));
    } else {
        snprintf(timestamp, sizeof(timestamp), "0000-00-00 00:00:00");
    }

    ESP_LOGI(TAG, "Medicion: %s → %.1f mm", timestamp, rain_mm);

    char csv_line[64];
    build_csv_line(timestamp, rain_mm, csv_line, sizeof(csv_line));
    sdcard_append_line(MAIN_LOG_FILE, csv_line);

    bool hay_pendientes = sdcard_count_records(TEMP_QUEUE_FILE) > 0;

    if (hay_pendientes) {
        ESP_LOGW(TAG, "Hay pendientes. Guardando en temp.csv.");
        sdcard_append_line(TEMP_QUEUE_FILE, csv_line);
    } else {
        bool sent = send_to_server(timestamp, rain_mm);
        if (!sent) {
            ESP_LOGW(TAG, "Fallo envio. Guardando en temp.csv.");
            sdcard_append_line(TEMP_QUEUE_FILE, csv_line);
        } else {
            ESP_LOGI(TAG, "Enviado correctamente.");
        }
    }

    ret = modbus_reset_rain();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo resetear registro de lluvia.");
    }
}

static void procesar_cola(void)
{
    if (sdcard_count_records(TEMP_QUEUE_FILE) == 0) return;

    char record[64] = {0};
    if (sdcard_read_first_record(TEMP_QUEUE_FILE, record, sizeof(record)) != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo leer primer pendiente.");
        return;
    }

    ESP_LOGI(TAG, "Reintentando pendiente: %s", record);

    char timestamp[24] = {0};
    float rain_mm = 0.0f;
    int parsed = sscanf(record, "%23[^,],%f", timestamp, &rain_mm);

    if (parsed != 2) {
        ESP_LOGW(TAG, "Formato invalido en cola: %s", record);
        sdcard_remove_first_record(TEMP_QUEUE_FILE, CSV_HEADER);
        return;
    }

    bool sent = send_to_server(timestamp, rain_mm);
    if (sent) {
        ESP_LOGI(TAG, "Pendiente enviado. Eliminando de cola.");
        sdcard_remove_first_record(TEMP_QUEUE_FILE, CSV_HEADER);
    } else {
        ESP_LOGW(TAG, "Fallo reenvio. Cola intacta.");
    }
}

static void init_all(void)
{
    ESP_LOGI(TAG, "══════════════════════════════════");
    ESP_LOGI(TAG, "  Estacion de lluvia — v1.0");
    ESP_LOGI(TAG, "══════════════════════════════════");

    ESP_ERROR_CHECK(ds3231_init());
    ESP_ERROR_CHECK(ds3231_enable_sqw_1hz());

    ESP_ERROR_CHECK(sdcard_init());
    sdcard_ensure_file(MAIN_LOG_FILE, CSV_HEADER);
    sdcard_ensure_file(TEMP_QUEUE_FILE, CSV_HEADER);

    ESP_ERROR_CHECK(modbus_rtu_init());
    ESP_ERROR_CHECK(dtu_init());

    ds3231_datetime_t dt;
    if (ds3231_get_datetime(&dt) == ESP_OK) {
        char ts[24];
        format_timestamp(&dt, ts, sizeof(ts));
        ESP_LOGI(TAG, "Hora actual: %s", ts);
    }

    ESP_LOGI(TAG, "Pendientes en cola: %lu", sdcard_count_records(TEMP_QUEUE_FILE));
    ESP_LOGI(TAG, "Sistema listo.");
}

void app_main(void)
{
    init_all();

    while (1) {
        ds3231_datetime_t dt;
        if (ds3231_get_datetime(&dt) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(LOOP_IDLE_MS));
            continue;
        }

        uint32_t slot = 0;
        if (is_measurement_due(&dt, &slot)) {
            last_slot_epoch = slot;
            tomar_medicion(slot);
        } else {
            uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now_ms - last_queue_retry > QUEUE_RETRY_INTERVAL_MS) {
                procesar_cola();
                last_queue_retry = now_ms;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_IDLE_MS));
    }
}