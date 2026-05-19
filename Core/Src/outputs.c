#include "outputs.h"
#include "state_store.h"
#include "main.h"

/* In-memory shadow of MQTT-managed output states. Bits match outputs.h. */
static uint8_t s_current_state = 0;

static inline void pin_set_active_low(GPIO_TypeDef *port, uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* Drive every MQTT-managed output to match shadow state. */
static void apply_shadow(void)
{
    pin_set_active_low(GPIOE, GPIO_PIN_13, (s_current_state & OUT_LED1)  ? 1 : 0);
    pin_set_active_low(GPIOE, GPIO_PIN_14, (s_current_state & OUT_LED2)  ? 1 : 0);
    pin_set_active_low(GPIOD, GPIO_PIN_4,  (s_current_state & OUT_RELAY) ? 1 : 0);
    /* PE15 (LED3) is NOT touched here — owned by heartbeat. */
}

void outputs_restore_from_nvm(void)
{
    s_current_state = state_store_load() & OUT_ALL;  /* mask off legacy bits */
    apply_shadow();
}

void outputs_apply(uint8_t mask, uint8_t state)
{
    if (state) s_current_state |=  (mask & OUT_ALL);
    else       s_current_state &= ~(mask & OUT_ALL);

    apply_shadow();
    state_store_save(s_current_state);
}

void outputs_fail_safe(void)
{
    s_current_state = 0;
    apply_shadow();
    state_store_save(0);
}

/* --- Heartbeat LED (LED3 = PE15, active-LOW) --- */
void heartbeat_led_on (void) { HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_RESET); }
void heartbeat_led_off(void) { HAL_GPIO_WritePin(GPIOE, GPIO_PIN_15, GPIO_PIN_SET);   }
