#include "rtc.h"
#include "rs485_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DS3231";

#define DS3231_ADDR         0x68
#define DS3231_REG_TIME     0x00
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_TEMP     0x11

static i2c_master_bus_handle_t  i2c_bus = NULL;
static i2c_master_dev_handle_t  i2c_dev = NULL;

static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
static uint8_t bcd2dec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

static esp_err_t ds3231_write_reg(uint8_t reg, uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = reg;
    for (size_t i = 0; i < len; i++) buf[i + 1] = data[i];
    return i2c_master_transmit(i2c_dev, buf, len + 1, pdMS_TO_TICKS(100));
}

static esp_err_t ds3231_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    esp_err_t ret = i2c_master_transmit(i2c_dev, &reg, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;
    return i2c_master_receive(i2c_dev, data, len, pdMS_TO_TICKS(100));
}

esp_err_t ds3231_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = RTC_I2C_SDA,
        .scl_io_num        = RTC_I2C_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = DS3231_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &i2c_dev));

    gpio_config_t sqw_cfg = {
        .pin_bit_mask = (1ULL << RTC_SQW_INT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&sqw_cfg));

    ESP_LOGI(TAG, "DS3231 inicializado — SDA:IO%d SCL:IO%d SQW:IO%d",
             RTC_I2C_SDA, RTC_I2C_SCL, RTC_SQW_INT);
    return ESP_OK;
}

esp_err_t ds3231_enable_sqw_1hz(void)
{
    uint8_t ctrl = 0x00;
    esp_err_t ret = ds3231_write_reg(DS3231_REG_CONTROL, &ctrl, 1);
    if (ret == ESP_OK) ESP_LOGI(TAG, "SQW configurado a 1Hz");
    return ret;
}

esp_err_t ds3231_set_datetime(const ds3231_datetime_t *dt)
{
    uint8_t buf[7] = {
        dec2bcd(dt->segundos),
        dec2bcd(dt->minutos),
        dec2bcd(dt->horas),
        dec2bcd(dt->dia),
        dec2bcd(dt->fecha),
        dec2bcd(dt->mes),
        dec2bcd(dt->anio),
    };
    esp_err_t ret = ds3231_write_reg(DS3231_REG_TIME, buf, 7);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Fecha/hora configurada: 20%02d-%02d-%02d %02d:%02d:%02d",
                 dt->anio, dt->mes, dt->fecha,
                 dt->horas, dt->minutos, dt->segundos);
    }
    return ret;
}

esp_err_t ds3231_get_datetime(ds3231_datetime_t *dt)
{
    uint8_t buf[7];
    esp_err_t ret = ds3231_read_reg(DS3231_REG_TIME, buf, 7);
    if (ret != ESP_OK) return ret;

    dt->segundos = bcd2dec(buf[0] & 0x7F);
    dt->minutos  = bcd2dec(buf[1] & 0x7F);
    dt->horas    = bcd2dec(buf[2] & 0x3F);
    dt->dia      = bcd2dec(buf[3] & 0x07);
    dt->fecha    = bcd2dec(buf[4] & 0x3F);
    dt->mes      = bcd2dec(buf[5] & 0x1F);
    dt->anio     = bcd2dec(buf[6]);

    return ESP_OK;
}

esp_err_t ds3231_get_temperature(float *temp_c)
{
    uint8_t buf[2];
    esp_err_t ret = ds3231_read_reg(DS3231_REG_TEMP, buf, 2);
    if (ret != ESP_OK) return ret;

    int8_t  temp_int  = (int8_t)buf[0];
    uint8_t temp_frac = (buf[1] >> 6) * 25;
    *temp_c = temp_int + (temp_frac / 100.0f);
    return ESP_OK;
}

void ds3231_print_datetime(const ds3231_datetime_t *dt)
{
    ESP_LOGI(TAG, "Fecha: 20%02d-%02d-%02d  Hora: %02d:%02d:%02d",
             dt->anio, dt->mes, dt->fecha,
             dt->horas, dt->minutos, dt->segundos);
}