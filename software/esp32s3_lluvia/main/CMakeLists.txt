idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES rtc sdcard modbus_rtu dtu config
)
