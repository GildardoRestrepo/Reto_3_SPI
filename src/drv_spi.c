/**
 * @file    drv_spi.c
 * @brief   API de alto nivel del driver SPI: tramas completas sobre ll_spi.
 *
 * Aquí no hay registros. La capa ll_spi transfiere bytes sueltos y maneja
 * CS; esta capa los agrupa en tramas (comando + datos) y traduce el error
 * del bajo nivel a SPI_Status_t.
 */

#include <stddef.h>
#include "drv_spi.h"
#include "ll_spi.h"

/* Byte que se envía solo para generar los 8 pulsos de reloj de una lectura */
#define SPI_DUMMY_BYTE   0x00U

/* ========================================================================== */
/*  Funciones auxiliares                                                      */
/* ========================================================================== */

/* Traduce el error guardado en ll_spi al estado del driver */
static SPI_Status_t SPI_Status(void)
{
    return (LL_SPI_GetError() == LL_SPI_ERR_NONE) ? SPI_OK : SPI_ERROR;
}

/* ========================================================================== */
/*  Funciones públicas                                                        */
/* ========================================================================== */

void SPI_Init(void)
{
    LL_SPI_Init();
}

SPI_Status_t SPI_WriteRegister(uint8_t cmd, uint8_t value)
{
    LL_SPI_CS_Low();
    (void)LL_SPI_TransferByte(cmd);     /* Lo que llega aquí no sirve */
    (void)LL_SPI_TransferByte(value);
    LL_SPI_CS_High();

    return SPI_Status();
}

SPI_Status_t SPI_ReadRegisters(uint8_t cmd, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    if ((buffer == NULL) || (length == 0U)) {
        return SPI_ERROR;
    }

    LL_SPI_CS_Low();
    (void)LL_SPI_TransferByte(cmd);     /* Mientras sale el comando, el
                                           esclavo aún no tiene datos */
    for (i = 0U; i < length; i++) {
        buffer[i] = LL_SPI_TransferByte(SPI_DUMMY_BYTE);
    }
    LL_SPI_CS_High();

    return SPI_Status();
}
