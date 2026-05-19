/*
 * Persistent output state stored in AT24C02 EEPROM (256-byte I2C device on
 * board JZ-F407VET6, addresses 0xA0/0xA1, accessed via bit-bang I2C on
 * PB8/PB9 — see eeprom_at24c02.c).
 *
 * Survives: software reset, IWDG reset, brown-out, FULL POWER LOSS.
 *
 * Layout (5 bytes at the start of EEPROM):
 *   [0..3]  magic = 0xCAFEF00D (validates that we wrote it)
 *   [4]     state byte (bit layout per outputs.h)
 *
 * In-RAM cache avoids hammering the EEPROM with reads — only one read at
 * boot, subsequent loads come from cache. Writes go straight to EEPROM so
 * the device survives an immediate power cut after the command.
 */

#include "state_store.h"
#include "eeprom_at24c02.h"

#define MAGIC_ADDR    0x00u
#define MAGIC_LEN     4u
#define STATE_ADDR    0x04u

static const uint8_t MAGIC_BYTES[MAGIC_LEN] = { 0x0D, 0xF0, 0xFE, 0xCA }; /* little-endian 0xCAFEF00D */

static uint8_t s_cached_state = 0;
static uint8_t s_initialised  = 0;

void state_store_init(void)
{
    at24c02_init();

    /* Validate magic. If absent, write it and start from clean state. */
    uint8_t magic[MAGIC_LEN] = {0};
    if (!at24c02_read(MAGIC_ADDR, magic, MAGIC_LEN) ||
        magic[0] != MAGIC_BYTES[0] || magic[1] != MAGIC_BYTES[1] ||
        magic[2] != MAGIC_BYTES[2] || magic[3] != MAGIC_BYTES[3])
    {
        at24c02_write(MAGIC_ADDR, MAGIC_BYTES, MAGIC_LEN);
        uint8_t zero = 0;
        at24c02_write(STATE_ADDR, &zero, 1);
        s_cached_state = 0;
        s_initialised  = 1;
        return;
    }

    /* Magic valid → pull state into cache. */
    uint8_t st = 0;
    if (at24c02_read(STATE_ADDR, &st, 1)) {
        s_cached_state = st;
    } else {
        s_cached_state = 0;
    }
    s_initialised = 1;
}

uint8_t state_store_load(void)
{
    /* state_store_init() must be called once first — but be defensive. */
    if (!s_initialised) state_store_init();
    return s_cached_state;
}

void state_store_save(uint8_t state)
{
    if (!s_initialised) state_store_init();
    if (state == s_cached_state) return;     /* no change → no write */

    if (at24c02_write(STATE_ADDR, &state, 1)) {
        s_cached_state = state;
    }
    /* If write fails, cache is not updated — next save() will retry. */
}
