/**
 * @file    ll_spi.c
 * @brief   Capa de bajo nivel del driver SPI: registros de RCC, GPIOB y SPI2.
 *
 * No usa HAL, LL ni las cabeceras CMSIS del fabricante: los registros se
 * definen aquí mismo a partir del manual de referencia RM0090, así que el
 * archivo compila en un proyecto "Empty" de STM32CubeIDE sin agregar nada.
 *
 * Referencias (RM0090 Rev 22, en _docs/datasheets/dm00031020.pdf):
 *   - Tabla 1      Direcciones base de los periféricos
 *   - Sec. 7.3     Registros del RCC
 *   - Sec. 8.4     Registros de los GPIO
 *   - Sec. 28.3.3  Configuración del SPI en modo maestro
 *   - Sec. 28.3.8  Procedimiento antes de terminar una transferencia
 *   - Sec. 28.5    Registros del SPI
 */

#include "ll_spi.h"

/* ========================================================================== */
/*  Mapa de registros                                                         */
/* ========================================================================== */

typedef struct {
    volatile uint32_t CR;            /* 0x00 */
    volatile uint32_t PLLCFGR;       /* 0x04 */
    volatile uint32_t CFGR;          /* 0x08 */
    volatile uint32_t CIR;           /* 0x0C */
    volatile uint32_t AHB1RSTR;      /* 0x10 */
    volatile uint32_t AHB2RSTR;      /* 0x14 */
    volatile uint32_t AHB3RSTR;      /* 0x18 */
    uint32_t          RESERVED0;     /* 0x1C */
    volatile uint32_t APB1RSTR;      /* 0x20  Reset de periféricos APB1   */
    volatile uint32_t APB2RSTR;      /* 0x24 */
    uint32_t          RESERVED1[2];  /* 0x28, 0x2C */
    volatile uint32_t AHB1ENR;       /* 0x30  Relojes de los GPIO         */
    volatile uint32_t AHB2ENR;       /* 0x34 */
    volatile uint32_t AHB3ENR;       /* 0x38 */
    uint32_t          RESERVED2;     /* 0x3C */
    volatile uint32_t APB1ENR;       /* 0x40  Reloj de SPI2               */
    volatile uint32_t APB2ENR;       /* 0x44 */
} RCC_TypeDef;

typedef struct {
    volatile uint32_t MODER;         /* 0x00  Modo: 2 bits por pin        */
    volatile uint32_t OTYPER;        /* 0x04  Tipo de salida: 1 bit       */
    volatile uint32_t OSPEEDR;       /* 0x08  Velocidad: 2 bits por pin   */
    volatile uint32_t PUPDR;         /* 0x0C  Pull-up/down: 2 bits        */
    volatile uint32_t IDR;           /* 0x10 */
    volatile uint32_t ODR;           /* 0x14 */
    volatile uint32_t BSRR;          /* 0x18  Set [15:0] / Reset [31:16]  */
    volatile uint32_t LCKR;          /* 0x1C */
    volatile uint32_t AFR[2];        /* 0x20 AFRL (pines 0-7), 0x24 AFRH (8-15) */
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CR1;           /* 0x00  Control 1                   */
    volatile uint32_t CR2;           /* 0x04  Control 2 (IRQ y DMA)       */
    volatile uint32_t SR;            /* 0x08  Estado                      */
    volatile uint32_t DR;            /* 0x0C  Datos                       */
    volatile uint32_t CRCPR;         /* 0x10 */
    volatile uint32_t RXCRCR;        /* 0x14 */
    volatile uint32_t TXCRCR;        /* 0x18 */
    volatile uint32_t I2SCFGR;       /* 0x1C */
    volatile uint32_t I2SPR;         /* 0x20 */
} SPI_TypeDef;

#define RCC     ((RCC_TypeDef  *)0x40023800UL)
#define GPIOB   ((GPIO_TypeDef *)0x40020400UL)
#define SPI2    ((SPI_TypeDef  *)0x40003800UL)

/* --- Bits del RCC --------------------------------------------------------- */
#define RCC_AHB1ENR_GPIOBEN    (1UL << 1)
#define RCC_APB1ENR_SPI2EN     (1UL << 14)
#define RCC_APB1RSTR_SPI2RST   (1UL << 14)

/* --- Bits de SPI_CR1 ------------------------------------------------------ */
#define SPI_CR1_CPHA           (1UL << 0)   /* Fase del reloj                 */
#define SPI_CR1_CPOL           (1UL << 1)   /* Polaridad del reloj            */
#define SPI_CR1_MSTR           (1UL << 2)   /* Modo maestro                   */
#define SPI_CR1_BR_POS         3U           /* BR[2:0]: divisor de fPCLK      */
#define SPI_CR1_SPE            (1UL << 6)   /* Habilita el periférico         */
#define SPI_CR1_SSI            (1UL << 8)   /* Nivel interno de NSS           */
#define SPI_CR1_SSM            (1UL << 9)   /* NSS manejado por software      */

/* --- Bits de SPI_SR ------------------------------------------------------- */
#define SPI_SR_RXNE            (1UL << 0)   /* Buffer de recepción con dato   */
#define SPI_SR_TXE             (1UL << 1)   /* Buffer de transmisión vacío    */
#define SPI_SR_BSY             (1UL << 7)   /* Transferencia en curso         */

/* ========================================================================== */
/*  Configuración de esta aplicación                                          */
/* ========================================================================== */

#define CS_PIN        12U       /* PB12: Chip Select (GPIO)                   */
#define SCK_PIN       13U       /* PB13: SPI2_SCK                             */
#define MISO_PIN      14U       /* PB14: SPI2_MISO                            */
#define MOSI_PIN      15U       /* PB15: SPI2_MOSI                            */

#define SPI2_AF       5UL       /* SPI2 es la función alternativa 5           */
#define SPI_BR_DIV16  3UL       /* BR = 011 -> fPCLK1/16 = 1 MHz con HSI      */

/* Vueltas máximas esperando una bandera. Un byte a 1 MHz tarda 8 us, o sea
 * unas pocas vueltas; 10000 da un margen de varios milisegundos. */
#define SPI_TIMEOUT   10000UL

/* Valores de los campos de 2 bits de los GPIO */
#define GPIO_MODE_OUT      1UL  /* MODER   = 01: salida                       */
#define GPIO_MODE_AF       2UL  /* MODER   = 10: función alternativa          */
#define GPIO_SPEED_MEDIUM  1UL  /* OSPEEDR = 01: velocidad media              */
#define GPIO_PULL_UP       1UL  /* PUPDR   = 01: pull-up                      */

/* Ubican un valor en el campo de un pin */
#define FIELD2(pin, val)   ((uint32_t)(val) << ((pin) * 2U))          /* 2 bits */
#define FIELD_AFRH(pin, val) ((uint32_t)(val) << (((pin) - 8U) * 4U)) /* 4 bits */

/* ========================================================================== */
/*  Estado interno                                                            */
/* ========================================================================== */

static uint8_t spi_error = LL_SPI_ERR_NONE;

/**
 * Espera a que los bits de 'mask' en SPI_SR valgan 'expected'.
 * Devuelve 1 si ocurrió y 0 si se agotó el tiempo (y guarda el error).
 * Con esto ninguna función del driver puede quedarse bloqueada para siempre.
 */
static uint8_t SPI_WaitSR(uint32_t mask, uint32_t expected)
{
    uint32_t tries = SPI_TIMEOUT;

    while ((SPI2->SR & mask) != expected) {
        if (--tries == 0U) {
            spi_error = LL_SPI_ERR_TIMEOUT;
            return 0U;
        }
    }
    return 1U;
}

/* ========================================================================== */
/*  Funciones públicas                                                        */
/* ========================================================================== */

void LL_SPI_Init(void)
{
    uint32_t reg;

    /* --- 1. Relojes ------------------------------------------------------- */
    /* Todo periférico arranca con el reloj apagado: sin esto, escribir en
     * sus registros no tiene efecto. GPIOB cuelga de AHB1 y SPI2 de APB1. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    (void)RCC->APB1ENR;     /* Lectura de relleno: el reloj tarda un par de
                               ciclos en quedar activo (errata ES0182, 2.2.13) */

    /* Reset de SPI2: lo deja con los valores de fábrica (CR1 = CR2 = 0,
     * modo SPI y no I2S). Así Init siempre parte del mismo estado y también
     * sirve para recuperar el periférico después de un error. */
    RCC->APB1RSTR |=  RCC_APB1RSTR_SPI2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;

    /* --- 2. Pines --------------------------------------------------------- */
    /* Los registros de GPIOB se modifican campo por campo (leer, limpiar,
     * escribir) para no dañar PB3/PB4, que traen la depuración (SWO/JTAG). */

    /* CS: primero el nivel y después el modo. Si se hiciera al revés, el pin
     * saldría un instante en bajo y el sensor vería una selección falsa. */
    GPIOB->BSRR = (1UL << CS_PIN);                      /* CS = 1 (reposo)    */

    /* Tipo de salida push-pull en CS, SCK y MOSI */
    GPIOB->OTYPER &= ~((1UL << CS_PIN) | (1UL << SCK_PIN) | (1UL << MOSI_PIN));

    /* Función alternativa 5 en SCK, MISO y MOSI (AFRH: pines 8 a 15) */
    reg  = GPIOB->AFR[1];
    reg &= ~(FIELD_AFRH(SCK_PIN, 0xFUL) | FIELD_AFRH(MISO_PIN, 0xFUL) |
             FIELD_AFRH(MOSI_PIN, 0xFUL));
    reg |=   FIELD_AFRH(SCK_PIN, SPI2_AF) | FIELD_AFRH(MISO_PIN, SPI2_AF) |
             FIELD_AFRH(MOSI_PIN, SPI2_AF);
    GPIOB->AFR[1] = reg;

    /* Velocidad media en SCK y MOSI: flancos suficientes para 1 MHz sin
     * generar rebotes de más en los cables. CS queda en velocidad baja. */
    reg  = GPIOB->OSPEEDR;
    reg &= ~(FIELD2(CS_PIN, 3UL) | FIELD2(SCK_PIN, 3UL) | FIELD2(MOSI_PIN, 3UL));
    reg |=   FIELD2(SCK_PIN, GPIO_SPEED_MEDIUM) | FIELD2(MOSI_PIN, GPIO_SPEED_MEDIUM);
    GPIOB->OSPEEDR = reg;

    /* Pull-up en SCK: en modo 3 el reloj reposa en alto y debe estar así
     * incluso antes de habilitar el SPI (nota de RM0090, sec. 28.3.1).
     * Pull-up en MISO: el sensor suelta esa línea cuando CS está en alto;
     * así no queda flotando y un sensor desconectado se lee como 0xFF. */
    reg  = GPIOB->PUPDR;
    reg &= ~(FIELD2(CS_PIN, 3UL) | FIELD2(SCK_PIN, 3UL) |
             FIELD2(MISO_PIN, 3UL) | FIELD2(MOSI_PIN, 3UL));
    reg |=   FIELD2(SCK_PIN, GPIO_PULL_UP) | FIELD2(MISO_PIN, GPIO_PULL_UP);
    GPIOB->PUPDR = reg;

    /* Modo: CS como salida; SCK, MISO y MOSI como función alternativa */
    reg  = GPIOB->MODER;
    reg &= ~(FIELD2(CS_PIN, 3UL) | FIELD2(SCK_PIN, 3UL) |
             FIELD2(MISO_PIN, 3UL) | FIELD2(MOSI_PIN, 3UL));
    reg |=   FIELD2(CS_PIN, GPIO_MODE_OUT) | FIELD2(SCK_PIN, GPIO_MODE_AF) |
             FIELD2(MISO_PIN, GPIO_MODE_AF) | FIELD2(MOSI_PIN, GPIO_MODE_AF);
    GPIOB->MODER = reg;

    /* --- 3. SPI2, con SPE = 0 --------------------------------------------- */
    /*  SSM = 1, SSI = 1 : NSS interno siempre en alto. CS es un GPIO normal;
     *                     sin esto el maestro detecta "fallo de modo" (MODF)
     *                     y el hardware le borra MSTR y SPE.
     *  MSTR = 1         : maestro (genera el reloj)
     *  CPOL = 1, CPHA = 1 : modo 3
     *  BR = 011         : fSCK = fPCLK1 / 16
     *  Lo que queda en 0: DFF (8 bits), LSBFIRST (MSB primero),
     *                     BIDIMODE y RXONLY (full-duplex), CRCEN (sin CRC).
     *  CR2 queda en 0 por el reset: sin interrupciones ni DMA (polling). */
    SPI2->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR |
                SPI_CR1_CPOL | SPI_CR1_CPHA |
                (SPI_BR_DIV16 << SPI_CR1_BR_POS);

    /* --- 4. Habilitar el periférico --------------------------------------- */
    SPI2->CR1 |= SPI_CR1_SPE;

    spi_error = LL_SPI_ERR_NONE;
}

uint8_t LL_SPI_TransferByte(uint8_t tx)
{
    /* 1. Esperar espacio en el buffer de transmisión */
    if (SPI_WaitSR(SPI_SR_TXE, SPI_SR_TXE) == 0U) {
        return 0xFFU;
    }

    /* 2. Escribir DR: el hardware genera los 8 pulsos de reloj */
    SPI2->DR = tx;

    /* 3. Esperar el byte que llega por MISO durante esos 8 pulsos */
    if (SPI_WaitSR(SPI_SR_RXNE, SPI_SR_RXNE) == 0U) {
        return 0xFFU;
    }

    /* 4. Leer DR: entrega el dato y limpia RXNE. Se lee siempre, aunque el
     *    dato no interese, para que nunca haya error de overrun (OVR). */
    return (uint8_t)SPI2->DR;
}

void LL_SPI_CS_Low(void)
{
    /* Mitad alta de BSRR = reset del pin. Es una sola escritura (atómica). */
    GPIOB->BSRR = (1UL << (CS_PIN + 16U));
}

void LL_SPI_CS_High(void)
{
    /* Subir CS con BSY = 1 cortaría el último byte (RM0090, sec. 28.3.8).
     * Si vence el tiempo se sube CS de todos modos para soltar al esclavo. */
    (void)SPI_WaitSR(SPI_SR_BSY, 0U);

    GPIOB->BSRR = (1UL << CS_PIN);
}

uint8_t LL_SPI_GetError(void)
{
    return spi_error;
}
