#ifndef __DEBOUNCER_H__
#define __DEBOUNCER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 8-sample shift-register button debouncer.
 *
 * Idea: feed one raw GPIO reading per call. Accept a state change only when
 * the last N samples all agree. N is configurable (1..8).
 *
 * Active-LOW convention (matches schematic with pull-ups R17/R18): a "press"
 * means GPIO read = 0. The API exposes the LOGICAL state (pressed=true).
 *
 * The debouncer is a pure module — no HAL or GPIO dependencies — so it can
 * be unit-tested on the host by feeding synthetic sample sequences. */

typedef struct {
    uint8_t history;       /* shift register, bit 0 = newest sample (active-LOW raw) */
    uint8_t stable;        /* current debounced state: 1 = pressed, 0 = released */
    uint8_t edge_pressed;  /* set to 1 by feed() when stable transitions 0→1 */
    uint8_t edge_released; /* set to 1 by feed() when stable transitions 1→0 */
    uint8_t threshold;     /* number of agreeing samples needed (typically 3..8) */
} debouncer_t;

/* Initialise. threshold must be 1..8. Initial state = released. */
void debouncer_init(debouncer_t *d, uint8_t threshold);

/* Feed one raw reading. raw_low_means_pressed: pass the GPIO bit directly
 * (HAL_GPIO_ReadPin returns 0 for LOW = pressed on active-low buttons). */
void debouncer_feed(debouncer_t *d, uint8_t raw_gpio_value);

/* Read & clear the "just pressed" edge flag. Returns true exactly once
 * per press event after debounce completes. */
bool debouncer_take_press(debouncer_t *d);

/* Read & clear the "just released" edge flag. */
bool debouncer_take_release(debouncer_t *d);

#ifdef __cplusplus
}
#endif

#endif
