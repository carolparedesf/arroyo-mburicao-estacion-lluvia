idf_component_register(
    SRCS "modbus_rtu.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_uart esp_driver_gpio config
)
