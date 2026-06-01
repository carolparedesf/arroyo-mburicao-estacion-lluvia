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
| Offset horario | UTC-3 (Paraguay) | `main.c` → `SERVER_TIME_OFFSET_SEC` |
| Slave ID sensor | 1 | `rs485_config.h` → `RAIN_SLAVE_ID` |
| Registro lluvia | 0x006F | `rs485_config.h` → `RAIN_REGISTER_ACCUMULATED` |
| Baudrate sensor | 9600 | `rs485_config.h` → `SENSOR_BAUD_RATE` |
| Baudrate DTU | 9600 | `rs485_config.h` → `DTU_BAUD_RATE` |

---

## Flujo de operación
### Secuencia de arranque
Al encender, el sistema inicializa todos los periféricos en orden:

1. **DS3231 RTC** (I2C) — referencia de tiempo para los slots de medición
2. **MicroSD** (SPI) — almacenamiento local de `datalog.csv` y `temp.csv`
3. **Modbus RTU** (UART1) — comunicación con el sensor de lluvia
4. **DTU** (UART2) — transmisión remota de datos

### Loop principal (cada 50 ms)

**Slot de medición (cada 60 s, sincronizado con RTC)**

1. Leer lluvia acumulada del sensor (Modbus FC03, registro `0x006F`)
2. Guardar lectura en `/datalog.csv` (siempre, como registro permanente)
3. Verificar cola pendiente en `/temp.csv`:
   - **Cola vacía** → intentar transmisión por DTU
     - Éxito → listo
     - Fallo → guardar en `/temp.csv`
   - **Cola con pendientes** → guardar en `/temp.csv` (DTU ocupado/no disponible)
4. Resetear registro de lluvia en el sensor (Modbus FC06)

**Entre slots de medición (cada 10 s)**

- Reintentar entradas pendientes en `/temp.csv` vía DTU
- Si tiene éxito → entrada eliminada de la cola

### Resumen del flujo de datos

```
Sensor de lluvia (RS485)
        │
        ▼
    ESP32-S3
        ├──→ /datalog.csv   (registro local permanente)
        └──→ DTU (RS485)
                 ├── OK   → base de datos en la nube
                 └── FAIL → /temp.csv → reintento cada 10s
```
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