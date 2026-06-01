|# Firmware ESP32-S3 — Estación de Lluvia

Firmware desarrollado en **ESP-IDF v6.1** para el ESP32-S3-WROOM-1.
Captura lluvia acumulada vía Modbus RTU, la almacena en MicroSD y la envía al servidor mediante DTU/RS485.

---

## Hardware

| Componente | Interfaz | GPIOs |
|------------|----------|-------|
| Sensor de lluvia | RS485 / Modbus RTU | TX:IO1, RX:IO2, DE/RE:IO4 |
| DTU / Modem | RS485 | TX:IO5, RX:IO6, DE/RE:IO7 |
| RTC DS3231M | I2C | SDA:IO8, SCL:IO9, SQW:IO16 |
| MicroSD | SPI | CS:IO10, MOSI:IO11, CLK:IO12, MISO:IO13, DET:IO15 |
| LED debug | GPIO | IO21 |

---

## Estructura del proyecto
---
esp32s3_lluvia/
├── main/
│   ├── main.c              # Pipeline principal
│   └── CMakeLists.txt
├── components/
│   ├── config/             # Pines y parámetros centralizados (rs485_config.h)
│   ├── rtc/                # Driver DS3231 por I2C
│   ├── sdcard/             # MicroSD vía SPI + FATFS
│   ├── modbus_rtu/         # Modbus RTU manual (CRC16, FC03, FC06)
│   ├── dtu/                # Comunicación con DTU por RS485
│   └── rs485/              # Driver RS485 genérico
├── CMakeLists.txt
└── sdkconfig
## Configuración rápida

| Parámetro | Valor | Archivo |
|-----------|-------|---------|
| Período de medición | 60 segundos | `main.c` → `MEASUREMENT_PERIOD_SEC` |
| Offset horario | UTC-4 (Paraguay) | `main.c` → `SERVER_TIME_OFFSET_SEC` |
| Slave ID sensor | 1 | `rs485_config.h` → `RAIN_SLAVE_ID` |
| Registro lluvia | 0x006F | `rs485_config.h` → `RAIN_REGISTER_ACCUMULATED` |
| Baudrate sensor | 9600 | `rs485_config.h` → `SENSOR_BAUD_RATE` |
| Baudrate DTU | 9600 | `rs485_config.h` → `DTU_BAUD_RATE` |

---

## Flujo de operación
Arranque
├── Init DS3231 (I2C)
├── Init MicroSD (SPI)
├── Init Modbus RTU (UART1)
└── Init DTU (UART2)
Loop cada 50ms
├── ¿Es momento de medir? (cada 60s, slot RTC)
│     ├── Leer lluvia acumulada (Modbus FC03, reg 0x006F)
│     ├── Guardar en /datalog.csv (siempre)
│     ├── ¿Hay pendientes en cola?
│     │     ├── SÍ → guardar en /temp.csv
│     │     └── NO → enviar por DTU
│     │           ├── OK → listo
│     │           └── FAIL → guardar en /temp.csv
│     └── Resetear registro de lluvia (Modbus FC06)
└── ¿No es momento de medir?
└── Reintentar cola /temp.csv (cada 10s)

---

## Archivos en MicroSD

| Archivo | Contenido |
|---------|-----------|
| `/datalog.csv` | Registro histórico completo de mediciones |
| `/temp.csv` | Cola FIFO de mediciones pendientes de envío |

Formato CSV:
Timestamp,Lluvia_mm
2026-05-22 17:41:00,1.5
---

## Compilar y flashear

```bash
# Activar entorno IDF
get_idf

# Entrar al proyecto
cd placa/firmware/esp32s3_lluvia

# Compilar
idf.py build

# Flashear y monitorear
idf.py -p /dev/ttyACM0 flash monitor
```

---

## Dependencias IDF

- `driver`, `esp_driver_uart`, `esp_driver_gpio`, `esp_driver_i2c`
- `fatfs`, `sdmmc`, `esp_driver_sdspi`
- `freertos`, `log`, `esp_common`

---

*PCB fabricado en JLCPCB — ESP32-S3-WROOM-1 — Mayo 2025*