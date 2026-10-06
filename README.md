# Reto 3 — Driver SPI Bare-Metal para STM32F407

**Curso:** Microprocesadores — Universidad Pontificia Bolivariana
**Integrantes:** Gildardo Estevan Restrepo · Marco Aurelio Guardia

Driver SPI escrito a nivel de registros (sin STM32Cube HAL ni LL) para la **STM32F407VET6**. La prueba de concepto lee el acelerómetro **GY-291 (ADXL345)** por SPI y muestra la aceleración en los ejes X, Y y Z por la consola **SWO** del depurador.

> Enunciado completo del reto: [docs/reto_3_spi.md](docs/reto_3_spi.md)

---

## 1. Descripción del protocolo

SPI (Serial Peripheral Interface) es un bus síncrono, full-duplex, maestro–esclavo, de 4 líneas:

| Línea | Función |
|-------|---------|
| SCK   | Reloj generado por el maestro |
| MOSI  | Datos maestro → esclavo |
| MISO  | Datos esclavo → maestro |
| CS    | Selección del esclavo (activo en bajo) |

Por cada byte que el maestro envía, recibe uno al mismo tiempo. El ADXL345 trabaja en **modo 3** (CPOL = 1, CPHA = 1) y acepta hasta 5 MHz.

La teoría detallada (arquitectura del periférico, buses, registros bit a bit) está en [docs/teoria.md](docs/teoria.md) *(pendiente)*.

---

## 2. Arquitectura del software

```
┌──────────────────────────────────────────┐
│  main.c          Aplicación (PoC)        │  Alto nivel
├──────────────────────────────────────────┤
│  adxl345.c/.h    Driver del sensor       │  Alto nivel
├──────────────────────────────────────────┤
│  drv_spi.c/.h    API pública SPI         │  Alto nivel
├──────────────────────────────────────────┤
│  ll_spi.c/.h     Registros RCC/GPIO/SPI  │  Bajo nivel
└──────────────────────────────────────────┘
   swo.c/.h        Salida por ITM/SWO (printf)
```

Regla de capas: cada capa solo llama a la inmediatamente inferior. Solo `ll_spi.c` y `swo.c` tocan registros.

### Estructura del repositorio

```
/inc     Cabeceras (.h)
/src     Código fuente (.c)
/docs    Enunciado, teoría y datasheets
```

---

## 3. Reparto de trabajo

| Etapa | Responsable | Archivos | Estado |
|-------|-------------|----------|--------|
| 1. Bajo nivel | Marco | `ll_spi.c/.h` | Pendiente |
| 2. Alto nivel | Gildardo | `drv_spi.c/.h`, `adxl345.c/.h`, `main.c` | Pendiente |
| 3. Visualización SWO | Marco | `swo.c/.h` | Pendiente |
| 4. Fundamentos teóricos | Gildardo | `docs/teoria.md`, este README | En curso |

Flujo de trabajo: una rama por etapa (`feat/bajo-nivel`, `feat/alto-nivel`, `feat/swo`, `docs/teoria`), Pull Request a `main` y revisión por el otro integrante antes del merge.

### Contrato entre capas (para trabajar en paralelo)

Interfaz que el bajo nivel expone al alto nivel. Si se cambia, se acuerda en el PR.

```c
/* ll_spi.h */
void    LL_SPI_Init(void);              /* Reloj, pines AF5, CS como salida, CR1 modo 3, SPE=1 */
uint8_t LL_SPI_TransferByte(uint8_t tx); /* Espera TXE, escribe DR, espera RXNE, lee DR */
void    LL_SPI_CS_Low(void);
void    LL_SPI_CS_High(void);

/* swo.h */
void    SWO_Init(void);                 /* Habilita ITM puerto 0; printf redirigido vía _write */
```

---

## 4. Hardware y conexiones

| GY-291 | STM32F407VET6 | Función |
|--------|---------------|---------|
| VCC    | 3V3           | Alimentación |
| GND    | GND           | Tierra |
| CS     | PB12          | Chip Select (GPIO salida) |
| SCL    | PB13          | SPI2_SCK (AF5) |
| SDO    | PB14          | SPI2_MISO (AF5) |
| SDA    | PB15          | SPI2_MOSI (AF5) |
| INT1/INT2 | —          | Sin conectar |

| Depurador | STM32F407VET6 |
|-----------|---------------|
| SWDIO / SWCLK / GND | PA13 / PA14 / GND |
| SWO       | PB3 |

Se usa **SPI2** (bus APB1) para no chocar con los LEDs de la placa en PA6/PA7 ni con la memoria W25Q16 conectada a SPI1.

*(Pendiente: esquemático en imagen en `docs/`.)*

---

## 5. Guía de uso de la API

*(Se completa al terminar la etapa de alto nivel.)* Funciones previstas:

| Función | Descripción |
|---------|-------------|
| `SPI_Init()` | Inicializa el periférico SPI2 en modo maestro, modo 3 |
| `SPI_TransmitReceive(data)` | Envía un byte y devuelve el byte recibido |
| `SPI_WriteRegister(reg, value)` | Escribe un registro de un esclavo SPI |
| `SPI_ReadRegisters(reg, buffer, length)` | Lee `length` registros consecutivos |
| `ADXL345_Init()` | Verifica `DEVID = 0xE5` y activa el modo medición |
| `ADXL345_ReadAccel(&x, &y, &z)` | Lee los tres ejes en mg |

Ejemplo de uso esperado:

```c
int main(void) {
    SWO_Init();
    SPI_Init();
    ADXL345_Init();

    int16_t x, y, z;
    while (1) {
        ADXL345_ReadAccel(&x, &y, &z);
        printf("X=%d Y=%d Z=%d mg\n", x, y, z);
    }
}
```

---

## 6. Compilación y ejecución

1. Abrir **STM32CubeIDE** → *File → New → STM32 Project* → seleccionar `STM32F407VETx` → tipo **Empty** (sin HAL).
2. Copiar `inc/` y `src/` al proyecto (o agregar las rutas en *Properties → C/C++ Build → Settings → Include paths*).
3. Compilar (*Project → Build*).
4. Configurar la depuración: *Debug Configurations → Debugger → Serial Wire Viewer (SWV) → Enable*, con **Core Clock = 16 MHz** (HSI por defecto).
5. Iniciar la depuración, abrir *Window → Show View → SWV → SWV ITM Data Console*, habilitar el puerto 0 y pulsar *Start Trace*.

> Requisito: el ST-Link debe tener el pin SWO cableado (muchos clones ST-Link V2 no lo tienen).
