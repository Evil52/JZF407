/*
 * Bit-bang I2C driver for AT24C02 EEPROM.
 *
 * Pins (per JZ-F407VET6 schematic):
 *   PB8 → SCL, with R5 = 4.7 kΩ pull-up to +3.3V
 *   PB9 → SDA, with R6 = 4.7 kΩ pull-up to +3.3V
 *
 * Why bit-bang instead of HAL_I2C:
 *   - HAL I2C source is not bundled with this STM32CubeCLT install
 *   - EEPROM access is rare and slow (writes happen on output state change)
 *   - Bit-bang is ~150 LOC, no dependencies, fully predictable timing
 *
 * Speed: ~50 kHz. AT24C02 is rated up to 400 kHz but for one byte every
 * few seconds we trade speed for code simplicity.
 */

#include "eeprom_at24c02.h"
#include "stm32f4xx_hal.h"

/* ── pin abstraction ─────────────────────────────────── */
#define SCL_PORT   GPIOB
#define SDA_PORT   GPIOB
#define SCL_PIN    GPIO_PIN_8
#define SDA_PIN    GPIO_PIN_9

/* Open-drain: LOW = drive low, HIGH = release (pull-up pulls line high).
 * On STM32 in GPIO_MODE_OUTPUT_OD this is achieved by WritePin SET/RESET. */
static inline void scl_high(void) { HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_SET);   }
static inline void scl_low (void) { HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, GPIO_PIN_RESET); }
static inline void sda_high(void) { HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_SET);   }
static inline void sda_low (void) { HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, GPIO_PIN_RESET); }
static inline uint8_t sda_read(void)
{
    return HAL_GPIO_ReadPin(SDA_PORT, SDA_PIN) == GPIO_PIN_SET ? 1u : 0u;
}

/* Short delay — calibrated for ~50 kHz I2C clock.
 * 168 MHz / 4 instructions per loop / 2 half-cycles ≈ need ~840 cycles per half.
 * NOP loop unrolled. Tune if EEPROM acts up. */
static inline void i2c_delay(void)
{
    for (volatile int i = 0; i < 30; i++) { __NOP(); }
}

/* ── primitives ──────────────────────────────────────── */
static void i2c_start(void)
{
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    sda_low();  i2c_delay();   /* SDA falls while SCL high → START */
    scl_low();  i2c_delay();
}

static void i2c_stop(void)
{
    sda_low();  i2c_delay();
    scl_high(); i2c_delay();
    sda_high(); i2c_delay();   /* SDA rises while SCL high → STOP */
}

/* Write 8 bits, sample ACK from slave. Returns 1 if ACKed. */
static uint8_t i2c_write_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        if ((b >> i) & 1) sda_high();
        else              sda_low();
        i2c_delay();
        scl_high(); i2c_delay();
        scl_low();  i2c_delay();
    }
    /* ACK bit: release SDA, read it during high SCL */
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    uint8_t ack = (sda_read() == 0) ? 1u : 0u;
    scl_low();  i2c_delay();
    return ack;
}

/* Read 8 bits, send ACK or NACK after them. */
static uint8_t i2c_read_byte(uint8_t ack_after)
{
    uint8_t b = 0;
    sda_high();    /* release line so slave can drive */
    for (int i = 7; i >= 0; i--) {
        i2c_delay();
        scl_high(); i2c_delay();
        if (sda_read()) b |= (1u << i);
        scl_low();
    }
    /* Master ACK/NACK */
    if (ack_after) sda_low();
    else           sda_high();
    i2c_delay();
    scl_high(); i2c_delay();
    scl_low();  i2c_delay();
    sda_high();
    return b;
}

/* ── public API ──────────────────────────────────────── */
void at24c02_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin   = SCL_PIN | SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;   /* open-drain, external pull-up */
    g.Pull  = GPIO_NOPULL;           /* board has 4.7K pull-ups */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    /* Idle state: both lines released */
    scl_high();
    sda_high();
}

#define AT24C02_ADDR_W  0xA0u  /* 7-bit 0x50, write */
#define AT24C02_ADDR_R  0xA1u  /* 7-bit 0x50, read  */

bool at24c02_read(uint8_t addr, uint8_t *data, uint8_t len)
{
    if (len == 0) return true;
    if (!data)    return false;

    /* Set internal address pointer */
    i2c_start();
    if (!i2c_write_byte(AT24C02_ADDR_W)) { i2c_stop(); return false; }
    if (!i2c_write_byte(addr))           { i2c_stop(); return false; }

    /* Repeated start, switch to read */
    i2c_start();
    if (!i2c_write_byte(AT24C02_ADDR_R)) { i2c_stop(); return false; }

    for (uint8_t i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i < (len - 1));  /* ACK except last byte */
    }
    i2c_stop();
    return true;
}

bool at24c02_write(uint8_t addr, const uint8_t *data, uint8_t len)
{
    if (len == 0) return true;
    if (!data)    return false;

    /* Single byte / single page write. Caller must not cross page (8-byte) boundary. */
    i2c_start();
    if (!i2c_write_byte(AT24C02_ADDR_W)) { i2c_stop(); return false; }
    if (!i2c_write_byte(addr))           { i2c_stop(); return false; }
    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_write_byte(data[i]))    { i2c_stop(); return false; }
    }
    i2c_stop();

    /* AT24C02 internal write cycle: tWR ≤ 5 ms. Block here so caller can
     * immediately read back and see the new value. */
    HAL_Delay(6);
    return true;
}
