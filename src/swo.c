/**
 * @file    swo.c
 * @brief   Salida de texto por SWO: pin PB3, TPIU, ITM y redirección de printf.
 *
 * Camino de un carácter:
 *   printf -> _write -> SWO_PrintChar -> ITM (puerto 0) -> TPIU -> PB3 -> J-Link
 *
 * El ITM arma un paquete por cada carácter y el TPIU lo saca en serie por el
 * pin TRACESWO a la velocidad que fija su prescaler.
 *
 * Junto con ll_spi.c es el único archivo que toca registros. No usa HAL, LL
 * ni CMSIS: las direcciones salen del manual de referencia.
 *
 * Referencias (RM0090 Rev 22, en _docs/datasheets/dm00031020.pdf):
 *   - Sec. 38.14     ITM: registros y ejemplo de configuración
 *   - Sec. 38.16.3   DBGMCU_CR: asignación de los pines de traza
 *   - Sec. 38.17     TPIU: modo asíncrono, registros y ejemplo (38.17.10)
 * El prescaler asíncrono del TPIU (ACPR, 0xE0040010) está en el manual de
 * arquitectura ARMv7-M de Arm.
 */

#include <stddef.h>
#include <stdint.h>
#include "swo.h"

/* ========================================================================== */
/*  Registros                                                                 */
/* ========================================================================== */

#define REG32(addr)     (*(volatile uint32_t *)(addr))
#define REG8(addr)      (*(volatile uint8_t  *)(addr))

/* --- Microcontrolador: reloj y pin ---------------------------------------- */
#define RCC_AHB1ENR     REG32(0x40023830UL)   /* Relojes de los GPIO          */
#define GPIOB_MODER     REG32(0x40020400UL)   /* Modo: 2 bits por pin         */
#define GPIOB_AFRL      REG32(0x40020420UL)   /* Función alternativa, pin 0-7 */
#define DBGMCU_CR       REG32(0xE0042004UL)   /* Control de depuración        */

/* --- Núcleo Cortex-M4: bloques de traza ----------------------------------- */
#define DEMCR           REG32(0xE000EDFCUL)   /* Habilita los bloques de traza */
#define TPIU_CSPSR      REG32(0xE0040004UL)   /* Tamaño del puerto de traza   */
#define TPIU_ACPR       REG32(0xE0040010UL)   /* Prescaler de la salida SWO   */
#define TPIU_SPPR       REG32(0xE00400F0UL)   /* Protocolo del pin            */
#define TPIU_FFCR       REG32(0xE0040304UL)   /* Formateador                  */
#define ITM_STIM0       REG32(0xE0000000UL)   /* Puerto de estímulo 0         */
#define ITM_STIM0_U8    REG8 (0xE0000000UL)   /* El mismo puerto, de 8 bits   */
#define ITM_TER         REG32(0xE0000E00UL)   /* Habilita puertos de estímulo */
#define ITM_TCR         REG32(0xE0000E80UL)   /* Control del ITM              */
#define ITM_LAR         REG32(0xE0000FB0UL)   /* Candado de escritura         */

/* --- Bits ----------------------------------------------------------------- */
#define RCC_AHB1ENR_GPIOBEN     (1UL << 1)
#define DEMCR_TRCENA            (1UL << 24)   /* Enciende ITM, DWT y TPIU      */
#define DBGMCU_CR_TRACE_IOEN    (1UL << 5)    /* Asigna los pines de traza     */
#define DBGMCU_CR_TRACE_MODE    (3UL << 6)    /* 00 = modo asíncrono (SWO)     */
#define TPIU_SPPR_NRZ           2UL           /* Serie NRZ, como una UART      */
#define TPIU_FFCR_TRIGIN        (1UL << 8)    /* Solo este bit: sin formateador */
#define ITM_LAR_KEY             0xC5ACCE55UL  /* Clave que abre el candado     */
#define ITM_TCR_ITMENA          (1UL << 0)    /* Habilita el ITM               */
#define ITM_TCR_SYNCENA         (1UL << 2)    /* Paquetes de sincronización    */
#define ITM_TCR_ATBID_1         (1UL << 16)   /* Identificador de la fuente    */
#define ITM_STIM_FIFOREADY      (1UL << 0)    /* Lectura: 1 = hay espacio      */

/* ========================================================================== */
/*  Configuración de esta aplicación                                          */
/* ========================================================================== */

#define SWO_PIN             3U          /* PB3: JTDO/TRACESWO                 */
#define SWO_AF              0UL         /* TRACESWO es la función alterna 0   */
#define GPIO_MODE_AF        2UL         /* MODER = 10: función alternativa    */

#define SWO_CPU_HZ          16000000UL  /* Reloj del núcleo (HSI)             */
#define SWO_BAUD_HZ         2000000UL   /* Velocidad de SWO                   */

/* Vueltas máximas esperando espacio en la cola del ITM. Un carácter tarda
 * unos 10 us en salir a 2 MHz; 10000 vueltas dan margen de sobra. */
#define SWO_TIMEOUT         10000UL

/* ========================================================================== */
/*  Funciones públicas                                                        */
/* ========================================================================== */

void SWO_Init(void)
{
    uint32_t reg;

    /* --- 1. Pin PB3 como TRACESWO (AF0) ----------------------------------- */
    /* Tras un reset PB3 ya viene así, pero se deja explícito para no
     * depender del estado anterior. Se modifica solo el campo de PB3. */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC_AHB1ENR;                  /* Lectura de relleno (errata ES0182) */

    reg  = GPIOB_AFRL;
    reg &= ~(0xFUL << (SWO_PIN * 4U));
    reg |=  (SWO_AF << (SWO_PIN * 4U));
    GPIOB_AFRL = reg;

    reg  = GPIOB_MODER;
    reg &= ~(3UL << (SWO_PIN * 2U));
    reg |=  (GPIO_MODE_AF << (SWO_PIN * 2U));
    GPIOB_MODER = reg;

    /* --- 2. Encender los bloques de traza --------------------------------- */
    /* Sin TRCENA, los registros del TPIU y del ITM se leen como cero y las
     * escrituras se pierden. */
    DEMCR |= DEMCR_TRCENA;

    /* --- 3. TPIU: cómo sale el dato por el pin ---------------------------- */
    TPIU_CSPSR = 1UL;                               /* Puerto de 1 bit        */
    TPIU_SPPR  = TPIU_SPPR_NRZ;                     /* Serie NRZ              */
    TPIU_ACPR  = (SWO_CPU_HZ / SWO_BAUD_HZ) - 1UL;  /* 16 MHz / 8 = 2 MHz     */
    TPIU_FFCR  = TPIU_FFCR_TRIGIN;                  /* Sin formateador: los
                                                       paquetes del ITM salen
                                                       tal cual               */

    /* --- 4. Asignar el pin de traza en modo asíncrono --------------------- */
    reg  = DBGMCU_CR;
    reg &= ~DBGMCU_CR_TRACE_MODE;                   /* TRACE_MODE = 00        */
    reg |=  DBGMCU_CR_TRACE_IOEN;
    DBGMCU_CR = reg;

    /* --- 5. ITM: quién genera los paquetes -------------------------------- */
    ITM_LAR = ITM_LAR_KEY;                          /* Abrir el candado       */
    ITM_TCR = ITM_TCR_ATBID_1 | ITM_TCR_SYNCENA | ITM_TCR_ITMENA;
    ITM_TER = 1UL;                                  /* Solo el puerto 0       */
}

void SWO_PrintChar(char c)
{
    uint32_t tries = SWO_TIMEOUT;

    /* Si nadie llamó a SWO_Init(), no hay a dónde escribir */
    if (((ITM_TCR & ITM_TCR_ITMENA) == 0U) || ((ITM_TER & 1UL) == 0U)) {
        return;
    }

    /* Leer el puerto dice si la cola tiene espacio (bit 0 = 1) */
    while ((ITM_STIM0 & ITM_STIM_FIFOREADY) == 0U) {
        if (--tries == 0U) {
            return;
        }
    }

    /* Escritura de 8 bits: el ITM genera un paquete de un solo byte */
    ITM_STIM0_U8 = (uint8_t)c;
}

void SWO_PrintString(const char *str)
{
    if (str == NULL) {
        return;
    }
    while (*str != '\0') {
        SWO_PrintChar(*str);
        str++;
    }
}

/**
 * Función que usa la biblioteca de C para escribir en stdout: printf() termina
 * aquí. El syscalls.c que genera STM32CubeIDE trae un _write "débil" (weak),
 * así que esta definición lo reemplaza sin tocar ese archivo.
 */
int _write(int file, char *ptr, int len);

int _write(int file, char *ptr, int len)
{
    int i;

    (void)file;
    for (i = 0; i < len; i++) {
        SWO_PrintChar(ptr[i]);
    }
    return len;
}
