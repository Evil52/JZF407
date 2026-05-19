#include "debouncer.h"

void debouncer_init(debouncer_t *d, uint8_t threshold)
{
    if (!d) return;
    if (threshold < 1) threshold = 1;
    if (threshold > 8) threshold = 8;
    d->history       = 0xFFu;   /* assume "released" (all HIGH) on startup */
    d->stable        = 0;
    d->edge_pressed  = 0;
    d->edge_released = 0;
    d->threshold     = threshold;
}

void debouncer_feed(debouncer_t *d, uint8_t raw_gpio_value)
{
    if (!d) return;

    /* Shift in newest raw sample. raw==0 means LOW = pressed (active-LOW). */
    d->history = (uint8_t)((d->history << 1) | (raw_gpio_value ? 1u : 0u));

    /* Mask of the last N samples we care about */
    uint8_t mask = (uint8_t)((1u << d->threshold) - 1u);
    uint8_t low_n  = (uint8_t)((d->history & mask) == 0);          /* N consecutive LOWs  → pressed */
    uint8_t high_n = (uint8_t)((d->history & mask) == mask);       /* N consecutive HIGHs → released */

    uint8_t new_stable = d->stable;
    if (low_n)  new_stable = 1;
    else if (high_n) new_stable = 0;

    if (new_stable != d->stable) {
        if (new_stable) d->edge_pressed  = 1;
        else            d->edge_released = 1;
        d->stable = new_stable;
    }
}

bool debouncer_take_press(debouncer_t *d)
{
    if (!d || !d->edge_pressed) return false;
    d->edge_pressed = 0;
    return true;
}

bool debouncer_take_release(debouncer_t *d)
{
    if (!d || !d->edge_released) return false;
    d->edge_released = 0;
    return true;
}
