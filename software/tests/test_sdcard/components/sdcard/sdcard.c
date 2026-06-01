#include "sdcard.h"
#include "rs485_config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdio.h>

static const char *TAG   = "SDCARD";
#define MOUNT_POINT        "/sdcard"
#define SPI_HOST           SPI2_HOST

static sdmmc_card_t *card = NULL;

esp_err_t sdcard_init(void)
{
    // Pin DET como entrada
    gpio_config_t det_cfg = {
        .pin_bit_mask = (1ULL << SD_DETECT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&det_cfg);

    if (!sdcard_is_inserted()) {
        ESP_LOGW(TAG, "No hay tarjeta SD insertada");
        return ESP_ERR_NOT_FOUND;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 5,
        .allocation_unit_size   = 16 * 1024,
    };

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs   = SD_SPI_CS;
    slot_cfg.host_id   = SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_SPI_MOSI,
        .miso_io_num = SD_SPI_MISO,
        .sclk_io_num = SD_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(SPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando SPI: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error montando SD: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD montada correctamente");
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

bool sdcard_is_inserted(void)
{
    // DET es activo en bajo cuando hay tarjeta
    return gpio_get_level(SD_DETECT) == 0;
}

esp_err_t sdcard_ensure_file(const char *path, const char *header)
{
    char full[64];
    snprintf(full, sizeof(full), "%s%s", MOUNT_POINT, path);

    FILE *f = fopen(full, "r");
    if (f) {
        fclose(f);
        return ESP_OK;  // ya existe
    }

    f = fopen(full, "w");
    if (!f) {
        ESP_LOGE(TAG, "No se pudo crear: %s", full);
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", header);
    fclose(f);
    ESP_LOGI(TAG, "Archivo creado: %s", full);
    return ESP_OK;
}

esp_err_t sdcard_append_line(const char *path, const char *line)
{
    char full[64];
    snprintf(full, sizeof(full), "%s%s", MOUNT_POINT, path);

    FILE *f = fopen(full, "a");
    if (!f) {
        ESP_LOGE(TAG, "No se pudo abrir para append: %s", full);
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", line);
    fclose(f);
    return ESP_OK;
}

esp_err_t sdcard_read_first_record(const char *path, char *buf, size_t buf_len)
{
    char full[64];
    snprintf(full, sizeof(full), "%s%s", MOUNT_POINT, path);

    FILE *f = fopen(full, "r");
    if (!f) return ESP_FAIL;

    // Saltar cabecera
    char tmp[128];
    fgets(tmp, sizeof(tmp), f);

    // Leer primer registro
    bool found = false;
    while (fgets(tmp, sizeof(tmp), f)) {
        size_t len = strlen(tmp);
        while (len > 0 && (tmp[len-1] == '\n' || tmp[len-1] == '\r')) tmp[--len] = 0;
        if (len == 0) continue;
        strncpy(buf, tmp, buf_len - 1);
        buf[buf_len - 1] = '\0';
        found = true;
        break;
    }

    fclose(f);
    return found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t sdcard_remove_first_record(const char *path, const char *header)
{
    char full[64];
    char aux[64];
    snprintf(full, sizeof(full), "%s%s",       MOUNT_POINT, path);
    snprintf(aux,  sizeof(aux),  "%s/temp_aux.csv", MOUNT_POINT);

    FILE *src = fopen(full, "r");
    if (!src) return ESP_FAIL;

    FILE *dst = fopen(aux, "w");
    if (!dst) { fclose(src); return ESP_FAIL; }

    fprintf(dst, "%s\n", header);

    char tmp[128];
    fgets(tmp, sizeof(tmp), src);  // saltar cabecera src

    bool skipped = false;
    while (fgets(tmp, sizeof(tmp), src)) {
        size_t len = strlen(tmp);
        while (len > 0 && (tmp[len-1] == '\n' || tmp[len-1] == '\r')) tmp[--len] = 0;
        if (len == 0) continue;
        if (!skipped) { skipped = true; continue; }  // saltar primer registro
        fprintf(dst, "%s\n", tmp);
    }

    fclose(src);
    fclose(dst);

    remove(full);
    rename(aux, full);

    ESP_LOGI(TAG, "Primer registro eliminado de %s", path);
    return ESP_OK;
}

uint32_t sdcard_count_records(const char *path)
{
    char full[64];
    snprintf(full, sizeof(full), "%s%s", MOUNT_POINT, path);

    FILE *f = fopen(full, "r");
    if (!f) return 0;

    char tmp[128];
    fgets(tmp, sizeof(tmp), f);  // saltar cabecera

    uint32_t count = 0;
    while (fgets(tmp, sizeof(tmp), f)) {
        size_t len = strlen(tmp);
        while (len > 0 && (tmp[len-1] == '\n' || tmp[len-1] == '\r')) tmp[--len] = 0;
        if (len > 0) count++;
    }

    fclose(f);
    return count;
}