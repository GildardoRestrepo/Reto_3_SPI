/**
 * @file    ll_spi.h
 * @brief   Capa de bajo nivel del driver SPI (Reto 3 - Microprocesadores UPB).
 *
 * Es la única capa que toca los registros de RCC, GPIOB y SPI2.
 * Las capas superiores (drv_spi, adxl345, main) solo usan estas funciones.
 *
 * Configuración fija:
 *   - SPI2 maestro, full-duplex, 8 bits, MSB primero
 *   - Modo 3 (CPOL = 1, CPHA = 1), el que exige el ADXL345
 *   - fSCK = fPCLK1 / 16  ->  1 MHz con el HSI de 16 MHz
 *   - PB13 = SCK, PB14 = MISO, PB15 = MOSI (AF5)
 *   - PB12 = CS, GPIO de salida manejado por software (activo en bajo)
 */

#ifndef LL_SPI_H
#define LL_SPI_H

#include <stdint.h>

/* Códigos que devuelve LL_SPI_GetError() */
#define LL_SPI_ERR_NONE     0U   /* Sin errores                                  */
#define LL_SPI_ERR_TIMEOUT  1U   /* Una bandera de SPI_SR no cambió a tiempo     */

/**
 * @brief Enciende relojes, configura los pines y deja SPI2 habilitado.
 *        CS queda en alto (esclavo sin seleccionar). Borra el error guardado.
 *        Se puede llamar de nuevo para recuperar el periférico tras un error.
 */
void LL_SPI_Init(void);

/**
 * @brief  Envía un byte y devuelve el que llega al mismo tiempo por MISO.
 * @param  tx  Byte a enviar (0x00 como "dummy" cuando solo se quiere leer).
 * @return Byte recibido. Si vence el tiempo de espera devuelve 0xFF y
 *         LL_SPI_GetError() pasa a LL_SPI_ERR_TIMEOUT.
 * @note   No toca CS: el que llama decide cuándo empieza y termina la trama.
 */
uint8_t LL_SPI_TransferByte(uint8_t tx);

/** @brief Baja CS (PB12): selecciona el esclavo e inicia una trama. */
void LL_SPI_CS_Low(void);

/** @brief Espera a que el bus quede libre (BSY = 0) y sube CS: fin de trama. */
void LL_SPI_CS_High(void);

/**
 * @brief  Devuelve el error guardado desde el último LL_SPI_Init().
 * @return LL_SPI_ERR_NONE o LL_SPI_ERR_TIMEOUT. No se borra al leerlo.
 */
uint8_t LL_SPI_GetError(void);

#endif /* LL_SPI_H */
