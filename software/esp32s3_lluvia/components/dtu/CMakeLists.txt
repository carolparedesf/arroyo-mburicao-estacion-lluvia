idf_component_register(
    SRCS "dtu.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_uart esp_driver_gpio config
)
