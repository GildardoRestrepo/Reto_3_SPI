/**
 * @file    drv_spi.h
 * @brief   API de alto nivel del driver SPI (Reto 3 - Microprocesadores UPB).
 *
 * Es la interfaz que usan los drivers de dispositivos (adxl345) y la
 * aplicación. No conoce registros: todo pasa por la capa ll_spi.
 *
 * Cada función arma una trama completa: baja CS, transfiere los bytes y
 * sube CS. Quien llama solo entrega el primer byte de la trama ("comando"),
 * ya armado con los bits de control que pida su esclavo.
 */

#ifndef DRV_SPI_H
#define DRV_SPI_H

#include <stdint.h>

/* Resultado de las operaciones del driver */
typedef enum {
    SPI_OK    = 0,   /* La trama se completó                                 */
    SPI_ERROR = 1    /* Venció un tiempo de espera o los parámetros no sirven */
} SPI_Status_t;

/**
 * @brief Deja el bus listo: SPI2 maestro, modo 3, 1 MHz, CS en alto.
 *        Llamarla de nuevo reinicia el periférico y borra los errores.
 */
void SPI_Init(void);

/**
 * @brief  Escribe un registro del esclavo con una trama de 2 bytes.
 * @param  cmd    Primer byte de la trama (dirección + bits de control).
 * @param  value  Valor a escribir.
 * @return SPI_OK o SPI_ERROR.
 */
SPI_Status_t SPI_WriteRegister(uint8_t cmd, uint8_t value);

/**
 * @brief  Envía el comando y lee 'length' bytes seguidos en la misma trama.
 * @param  cmd     Primer byte de la trama (dirección + bits de control).
 * @param  buffer  Destino de los bytes leídos.
 * @param  length  Cantidad de bytes a leer (al menos 1).
 * @return SPI_OK o SPI_ERROR.
 */
SPI_Status_t SPI_ReadRegisters(uint8_t cmd, uint8_t *buffer, uint8_t length);

#endif /* DRV_SPI_H */
