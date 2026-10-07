/**
 * @file    main.c
 * @brief   Prueba de concepto: lectura continua del ADXL345 por SPI2.
 *
 * Mientras no exista la salida por SWO, las lecturas se ven en el
 * depurador con Live Expressions (variables 'accel', 'lecturas', 'errores').
 */

#include <stdint.h>
#include "adxl345.h"

/* Pausa entre lecturas. Es un retardo por software: a 16 MHz cada vuelta
 * toma unos pocos ciclos, así que 400000 vueltas son del orden de 100 ms. */
#define DELAY_LOOPS   400000UL

/* Variables globales para verlas en Live Expressions */
volatile ADXL345_Accel_t accel;     /* Última lectura en mg                 */
volatile uint32_t lecturas = 0U;    /* Lecturas correctas                   */
volatile uint32_t errores  = 0U;    /* Fallos de init o de lectura          */
volatile ADXL345_Status_t estado;   /* Resultado de la última operación     */

static void Delay(uint32_t loops)
{
    volatile uint32_t i;

    for (i = 0U; i < loops; i++) {
    }
}

int main(void)
{
    ADXL345_Accel_t muestra;

    /* Reintenta hasta encontrar el sensor (útil si se conecta tarde) */
    while ((estado = ADXL345_Init()) != ADXL345_OK) {
        errores++;
        Delay(DELAY_LOOPS);
    }

    for (;;) {
        estado = ADXL345_ReadAccel(&muestra);

        if (estado == ADXL345_OK) {
            accel = muestra;
            lecturas++;
        } else {
            /* Un error de bus se recupera reiniciando SPI y sensor */
            errores++;
            (void)ADXL345_Init();
        }

        Delay(DELAY_LOOPS);
    }
}
