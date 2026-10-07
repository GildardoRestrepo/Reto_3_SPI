---
title: Reto 3 — Driver SPI Bare-Metal para STM32F407
created: 2026-10-06
time: 06:50pm
creator: Gilbert
last update: 2026-10-06
update by: Gilbert
type: readme
status: activo
fase: ""
area: microcontroladores
editor: Gilbert
order: 0
tags:
  - tipo/readme
---

# Reto 3 — Driver SPI Bare-Metal para STM32F407

> [!success] Resumen
> Driver SPI escrito a nivel de registros (sin STM32Cube HAL ni LL) para la **STM32F407VET6**. La prueba de concepto lee el acelerómetro **GY-291 (ADXL345)** y muestra la aceleración en X, Y y Z por la consola **SWO** del depurador.

**Curso:** Microprocesadores — Universidad Pontificia Bolivariana
**Integrantes:** Gildardo Estevan Restrepo · Marco Aurelio Guardia

---

## Estructura del repositorio

```
/inc     Cabeceras (.h)
/src     Código fuente (.c)
/_docs   Enunciado, teoría y datasheets
```

---

## Reparto de trabajo

| Etapa | Responsable | Estado |
|-------|-------------|--------|
| 1. Bajo nivel (registros) | Marco | Pendiente |
| 2. Alto nivel (API y sensor) | Gildardo | Pendiente |
| 3. Visualización por SWO | Marco | Pendiente |
| 4. Fundamentos teóricos | Gildardo | En curso |

Flujo de trabajo: una rama por etapa, Pull Request a `main` y revisión del otro integrante antes del merge.

---

## Hardware y conexiones

| GY-291 | STM32F407VET6 | Función |
|--------|---------------|---------|
| VCC    | 3V3           | Alimentación |
| GND    | GND           | Tierra |
| CS     | PB12          | Chip Select (GPIO salida) |
| SCL    | PB13          | SPI2_SCK (AF5) |
| SDO    | PB14          | SPI2_MISO (AF5) |
| SDA    | PB15          | SPI2_MOSI (AF5) |

| Depurador | STM32F407VET6 |
|-----------|---------------|
| SWDIO / SWCLK / GND | PA13 / PA14 / GND |
| SWO       | PB3 |

Se usa **SPI2** para no chocar con los LEDs de la placa (PA6/PA7) ni con la memoria W25Q16 conectada a SPI1.

---

## Enlaces

- [Enunciado del reto](_docs/reto_3_spi.md)
- [Teoría del protocolo SPI](_docs/teoria_spi.md)
- [Manual de referencia RM0090](_docs/datasheets/dm00031020.pdf)
- [Esquemático de la placa](_docs/datasheets/stm32f407vet6_schematics.pdf)
