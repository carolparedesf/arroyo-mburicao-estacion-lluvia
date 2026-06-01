idf_component_register(
    SRCS "sdcard.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_gpio fatfs sdmmc config
)
