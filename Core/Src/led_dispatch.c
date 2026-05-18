#include "led_dispatch.h"
#include <string.h>

#define LED1_MASK 0x01
#define LED2_MASK 0x02
#define LED3_MASK 0x04
#define LED_ALL   (LED1_MASK | LED2_MASK | LED3_MASK)

led_dispatch_t led_dispatch_parse(const char *topic,
                                  const unsigned char *payload,
                                  size_t payload_len)
{
    led_dispatch_t r = { 0, 0, 0 };

    if (!topic) return r;

    /* Echo topic — no LED change, just request echo back */
    if (strcmp(topic, "stm32/ping") == 0) {
        r.echo = 1;
        return r;
    }

    /* All LED topics need a payload of at least 1 byte */
    if (payload_len == 0 || !payload) return r;

    r.state = (payload[0] == '1') ? 1 : 0;

    if      (strcmp(topic, "stm32/led/all") == 0) r.mask = LED_ALL;
    else if (strcmp(topic, "stm32/led/1")   == 0) r.mask = LED1_MASK;
    else if (strcmp(topic, "stm32/led/2")   == 0) r.mask = LED2_MASK;
    else if (strcmp(topic, "stm32/led/3")   == 0) r.mask = LED3_MASK;
    /* else: unknown topic → mask stays 0, no action */

    return r;
}
