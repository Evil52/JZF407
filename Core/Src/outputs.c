#include "outputs.h"
#include "main.h"

/* Active-LOW LEDs on PE13/14/15. Centralised here so MQTT layer and
 * fail-safe path share the same logic. */
#define LED1_PIN  GPIO_PIN_13
#define LED2_PIN  GPIO_PIN_14
#define LED3_PIN  GPIO_PIN_15
#define LED_PORT  GPIOE

static inline void led_set(uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(LED_PORT, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void outputs_apply(uint8_t mask, uint8_t state)
{
    if (mask & 0x01) led_set(LED1_PIN, state);
    if (mask & 0x02) led_set(LED2_PIN, state);
    if (mask & 0x04) led_set(LED3_PIN, state);
}

void outputs_fail_safe(void)
{
    /* Force every managed output to OFF. Idempotent — safe to call repeatedly. */
    led_set(LED1_PIN, 0);
    led_set(LED2_PIN, 0);
    led_set(LED3_PIN, 0);
}
