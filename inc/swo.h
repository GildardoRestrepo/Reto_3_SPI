/**
 * @file    swo.h
 * @brief   Salida de texto por SWO (Serial Wire Output) usando el ITM.
 *
 * Reemplaza a una USART para ver mensajes: el texto sale por el pin PB3
 * (TRACESWO) hacia el depurador y se lee en la consola SWV del IDE.
 * Después de SWO_Init(), printf() también sale por aquí.
 *
 * Configuración fija:
 *   - Reloj del núcleo: 16 MHz (HSI)
 *   - Velocidad de SWO: 2 MHz, codificación NRZ (tipo UART)
 *   - Puerto de estímulo 0 del ITM
 *
 * En el depurador hay que poner los mismos valores (Core Clock = 16 MHz,
 * SWO Clock = 2000 kHz). Si no coinciden, la consola muestra basura.
 */

#ifndef SWO_H
#define SWO_H

/**
 * @brief Configura PB3 como TRACESWO y habilita TPIU e ITM (puerto 0).
 *        Debe llamarse antes del primer printf().
 */
void SWO_Init(void);

/**
 * @brief Envía un carácter por el puerto 0 del ITM.
 *        Si SWO no está inicializado o la cola no se desocupa a tiempo,
 *        el carácter se descarta: nunca bloquea el programa.
 */
void SWO_PrintChar(char c);

/** @brief Envía una cadena terminada en '\0'. */
void SWO_PrintString(const char *str);

#endif /* SWO_H */
