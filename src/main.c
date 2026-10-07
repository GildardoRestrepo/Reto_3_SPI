/**
 * @file    main.c
 * @brief   Prueba de concepto: lectura continua del ADXL345 por SPI2.
 *
 * Las lecturas salen por SWO con printf (consola SWV del depurador). Las
 * variables 'accel', 'lecturas', 'errores' y 'estado' siguen disponibles
 * para verlas con Live Expressions.
 */

#include <stdint.h>
#include <stdio.h>
#include "adxl345.h"
#include "swo.h"

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

/* Nombre del estado para mostrarlo en la consola */
static const char *EstadoTexto(ADXL345_Status_t s)
{
    switch (s) {
        case ADXL345_OK:         return "OK";
        case ADXL345_ERR_SPI:    return "ERR_SPI (tiempo de espera del bus)";
        case ADXL345_ERR_ID:     return "ERR_ID (sensor ausente o mal cableado)";
        case ADXL345_ERR_CONFIG: return "ERR_CONFIG (sensor desconectado o reiniciado)";
        default:                 return "desconocido";
    }
}

int main(void)
{
    ADXL345_Accel_t muestra;

    SWO_Init();
    printf("Reto 3 - SPI: buscando el ADXL345...\n");

    /* Reintenta hasta encontrar el sensor (útil si se conecta tarde) */
    while ((estado = ADXL345_Init()) != ADXL345_OK) {
        errores++;
        printf("Init: %s\n", EstadoTexto(estado));
        Delay(DELAY_LOOPS);
    }
    printf("ADXL345 listo: +/-16 g, 3.9 mg por LSB\n");

    for (;;) {
        estado = ADXL345_ReadAccel(&muestra);

        if (estado == ADXL345_OK) {
            accel = muestra;
            lecturas++;
            printf("X=%6d  Y=%6d  Z=%6d  mg\n",
                   muestra.x_mg, muestra.y_mg, muestra.z_mg);
        } else {
            /* Error de bus o sensor desconectado/reiniciado: se reinician
             * SPI y sensor. Si el sensor no está, Init falla y se vuelve a
             * intentar en la siguiente vuelta hasta que aparezca. */
            errores++;
            printf("Lectura: %s. Reiniciando sensor...\n", EstadoTexto(estado));
            (void)ADXL345_Init();
        }

        Delay(DELAY_LOOPS);
    }
}
