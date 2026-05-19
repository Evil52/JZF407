#include "outputs.h"
#include "main.h"

/* All managed outputs are active-LOW:
 *   LED:   anode → +3.3V via R, cathode → MCU pin
 *   Relay: SONGLE module pulls CHn LOW via opto-coupler → coil energises
 * Setting pin HIGH = OFF. Setting pin LOW = ON. */

static inline void pin_set_active_low(GPIO_TypeDef *port, uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void outputs_apply(uint8_t mask, uint8_t state)
{
    /* LEDs on PE13/14/15 */
    if (mask & OUT_LED1) pin_set_active_low(GPIOE, GPIO_PIN_13, state);
    if (mask & OUT_LED2) pin_set_active_low(GPIOE, GPIO_PIN_14, state);
    if (mask & OUT_LED3) pin_set_active_low(GPIOE, GPIO_PIN_15, state);

    /* External relays on P4 connector */
    if (mask & OUT_RELAY1) pin_set_active_low(GPIOA, GPIO_PIN_0, state);
    if (mask & OUT_RELAY2) pin_set_active_low(GPIOC, GPIO_PIN_0, state);
    if (mask & OUT_RELAY3) pin_set_active_low(GPIOA, GPIO_PIN_3, state);
}

void outputs_fail_safe(void)
{
    /* Force every managed output to OFF. Called when MQTT supervision is lost. */
    outputs_apply(OUT_LED_ALL | OUT_RELAY_ALL, 0);
}
