#ifndef __EEPROM_AT24C02_H__
#define __EEPROM_AT24C02_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AT24C02 EEPROM driver — software bit-bang I2C on PB8 (SCL) / PB9 (SDA).
 *
 * The board's pull-ups (R5, R6 = 4.7K) are already on these lines per the
 * JZ-F407VET6 schematic, so we drive the pins in open-drain mode (output
 * LOW or HiZ).
 *
 * AT24C02 datasheet:
 *   - 2 Kbit = 256 bytes, organised as 32 pages × 8 bytes
 *   - I2C address: 0b1010 A2 A1 A0  → 0xA0..0xAE (R/W bit appended)
 *     On JZ-F407VET6 A0..A2 are tied to GND → device address = 0xA0/0xA1
 *   - Write cycle: ≤5 ms (poll for ACK or just delay)
 *   - Page-write boundary: cannot cross an 8-byte page in a single write */

#define AT24C02_SIZE  256u

void  at24c02_init(void);

/* Returns true on success. data may be NULL only if len==0. */
bool  at24c02_read (uint8_t addr, uint8_t *data, uint8_t len);
bool  at24c02_write(uint8_t addr, const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
