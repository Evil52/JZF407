#include "buttons.h"
#include "debouncer.h"
#include "outputs.h"
#include "main.h"

#define S1_PORT   GPIOE
#define S1_PIN    GPIO_PIN_10
#define S2_PORT   GPIOE
#define S2_PIN    GPIO_PIN_11

#define DEBOUNCE_SAMPLES  4  /* 4 × 100 ms = 400 ms — safe against any bounce */

static debouncer_t s1_db;
static debouncer_t s2_db;

void buttons_init(void)
{
    /* GPIOs PE10/PE11 are already configured as INPUT by CubeMX in MX_GPIO_Init
     * (Pull = NOPULL, the board has hardware 10K pull-ups). Nothing else needed. */
    debouncer_init(&s1_db, DEBOUNCE_SAMPLES);
    debouncer_init(&s2_db, DEBOUNCE_SAMPLES);
}

void buttons_poll(void)
{
    /* HAL_GPIO_ReadPin returns 0 (RESET) when line is LOW (button pressed
     * because of the pull-up + ground-through-button topology). */
    debouncer_feed(&s1_db, HAL_GPIO_ReadPin(S1_PORT, S1_PIN));
    debouncer_feed(&s2_db, HAL_GPIO_ReadPin(S2_PORT, S2_PIN));

    if (debouncer_take_press(&s1_db)) {
        outputs_apply(OUT_RELAY, 1);   /* S1 → relay ON */
    }
    if (debouncer_take_press(&s2_db)) {
        outputs_apply(OUT_RELAY, 0);   /* S2 → relay OFF */
    }
    /* releases are intentionally ignored: tactile buttons trigger on press */
}
