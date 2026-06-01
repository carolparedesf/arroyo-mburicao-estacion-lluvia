idf_component_register(
    SRCS "rtc.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_gpio esp_driver_i2c config
)
