/**
 * @file    adxl345.h
 * @brief   Driver del acelerómetro ADXL345 (módulo GY-291) sobre drv_spi.
 *
 * Configuración fija:
 *   - Rango ±16 g con resolución completa: 3,9 mg por LSB (256 LSB/g)
 *   - Frecuencia de muestreo por defecto del sensor: 100 Hz
 *   - SPI de 4 hilos, modo 3
 */

#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>

/* Resultado de las operaciones del sensor */
typedef enum {
    ADXL345_OK      = 0,   /* Operación correcta                              */
    ADXL345_ERR_SPI = 1,   /* Falló la comunicación SPI (tiempo de espera)    */
    ADXL345_ERR_ID  = 2    /* DEVID no es 0xE5: sensor ausente o mal cableado */
} ADXL345_Status_t;

/* Aceleración en los tres ejes, en miligravedades (1000 mg = 1 g) */
typedef struct {
    int16_t x_mg;
    int16_t y_mg;
    int16_t z_mg;
} ADXL345_Accel_t;

/**
 * @brief  Inicializa el bus SPI, verifica el sensor y lo pone a medir.
 *         Se puede llamar de nuevo para recuperarse de un error.
 * @return ADXL345_OK, ADXL345_ERR_SPI o ADXL345_ERR_ID.
 */
ADXL345_Status_t ADXL345_Init(void);

/**
 * @brief  Lee los tres ejes de una misma muestra.
 * @param  accel  Destino de la lectura en mg.
 * @return ADXL345_OK o ADXL345_ERR_SPI. Si hay error, 'accel' no cambia.
 */
ADXL345_Status_t ADXL345_ReadAccel(ADXL345_Accel_t *accel);

#endif /* ADXL345_H */
