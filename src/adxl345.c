/**
 * @file    adxl345.c
 * @brief   Driver del acelerómetro ADXL345: registros del sensor y escala.
 *
 * Solo conoce el protocolo del sensor (formato del byte de comando y mapa
 * de registros); el bus lo maneja drv_spi.
 *
 * Referencia: hoja de datos ADXL345 (Analog Devices), secciones
 * "Serial Communications - SPI" y "Register Map".
 */

#include <stddef.h>
#include "adxl345.h"
#include "drv_spi.h"

/* ========================================================================== */
/*  Mapa de registros del sensor                                              */
/* ========================================================================== */

#define ADXL345_REG_DEVID        0x00U   /* Identificador fijo                */
#define ADXL345_REG_POWER_CTL    0x2DU   /* Modo de energía                   */
#define ADXL345_REG_DATA_FORMAT  0x31U   /* Rango y resolución                */
#define ADXL345_REG_DATAX0       0x32U   /* Primero de los 6 bytes de datos   */

#define ADXL345_DEVID_VALUE      0xE5U

/* --- Bits del byte de comando --------------------------------------------- */
#define ADXL345_CMD_READ         0x80U   /* Bit 7 R/W: 1 = lectura            */
#define ADXL345_CMD_MULTIBYTE    0x40U   /* Bit 6 MB: autoincremento          */

/* --- Valores de configuración --------------------------------------------- */
#define ADXL345_POWER_MEASURE    0x08U   /* POWER_CTL bit 3: modo medición    */
#define ADXL345_FORMAT_FULL_RES  0x08U   /* DATA_FORMAT bit 3: 3,9 mg/LSB     */
#define ADXL345_FORMAT_RANGE_16G 0x03U   /* DATA_FORMAT bits 1:0 = 11: ±16 g  */

/* Escala en resolución completa: 256 LSB por g -> mg = raw * 1000 / 256 */
#define ADXL345_LSB_PER_G        256L

/* ========================================================================== */
/*  Funciones auxiliares                                                      */
/* ========================================================================== */

/* Escribe un registro: bit R/W = 0 y MB = 0, el comando es la dirección */
static ADXL345_Status_t ADXL345_Write(uint8_t reg, uint8_t value)
{
    return (SPI_WriteRegister(reg, value) == SPI_OK) ? ADXL345_OK
                                                     : ADXL345_ERR_SPI;
}

/* Lee 'length' registros desde 'reg'. MB = 1 cuando son varios, para que
 * el sensor avance solo a la dirección siguiente en cada byte. */
static ADXL345_Status_t ADXL345_Read(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t cmd = ADXL345_CMD_READ | reg;

    if (length > 1U) {
        cmd |= ADXL345_CMD_MULTIBYTE;
    }
    return (SPI_ReadRegisters(cmd, buffer, length) == SPI_OK) ? ADXL345_OK
                                                              : ADXL345_ERR_SPI;
}

/* Une el byte bajo y el alto (complemento a 2) y lo pasa a mg */
static int16_t ADXL345_ToMg(uint8_t low, uint8_t high)
{
    int16_t raw = (int16_t)(((uint16_t)high << 8) | low);

    return (int16_t)(((int32_t)raw * 1000L) / ADXL345_LSB_PER_G);
}

/* ========================================================================== */
/*  Funciones públicas                                                        */
/* ========================================================================== */

ADXL345_Status_t ADXL345_Init(void)
{
    uint8_t devid;

    SPI_Init();

    /* 1. ¿Hay un ADXL345 al otro lado? Si MISO está suelto, el pull-up
     *    hace que se lea 0xFF y no 0xE5. */
    if (ADXL345_Read(ADXL345_REG_DEVID, &devid, 1U) != ADXL345_OK) {
        return ADXL345_ERR_SPI;
    }
    if (devid != ADXL345_DEVID_VALUE) {
        return ADXL345_ERR_ID;
    }

    /* 2. Formato: resolución completa y ±16 g. El bit SPI queda en 0
     *    (4 hilos) y el dato justificado a la derecha. */
    if (ADXL345_Write(ADXL345_REG_DATA_FORMAT,
                      ADXL345_FORMAT_FULL_RES | ADXL345_FORMAT_RANGE_16G)
        != ADXL345_OK) {
        return ADXL345_ERR_SPI;
    }

    /* 3. Pasar de reposo a medición. Se hace al final, con el formato ya
     *    configurado. */
    return ADXL345_Write(ADXL345_REG_POWER_CTL, ADXL345_POWER_MEASURE);
}

ADXL345_Status_t ADXL345_ReadAccel(ADXL345_Accel_t *accel)
{
    uint8_t data[6];   /* X0 X1 Y0 Y1 Z0 Z1 */

    if (accel == NULL) {
        return ADXL345_ERR_SPI;
    }

    /* Los 6 bytes en una sola trama: así los tres ejes son de la misma
     * muestra (la hoja de datos lo recomienda). */
    if (ADXL345_Read(ADXL345_REG_DATAX0, data, 6U) != ADXL345_OK) {
        return ADXL345_ERR_SPI;
    }

    accel->x_mg = ADXL345_ToMg(data[0], data[1]);
    accel->y_mg = ADXL345_ToMg(data[2], data[3]);
    accel->z_mg = ADXL345_ToMg(data[4], data[5]);

    return ADXL345_OK;
}
