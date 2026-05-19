#include "led_dispatch.h"
#include "outputs.h"
#include <string.h>

led_dispatch_t led_dispatch_parse(const char *topic,
                                  const unsigned char *payload,
                                  size_t payload_len)
{
    led_dispatch_t r = { 0, 0, 0 };

    if (!topic) return r;

    /* Echo topic — no output change, just request echo back */
    if (strcmp(topic, "stm32/ping") == 0) {
        r.echo = 1;
        return r;
    }

    /* Output topics need a payload of at least 1 byte */
    if (payload_len == 0 || !payload) return r;

    r.state = (payload[0] == '1') ? 1 : 0;

    /* LEDs (onboard) */
    if      (strcmp(topic, "stm32/led/all") == 0) r.mask = OUT_LED_ALL;
    else if (strcmp(topic, "stm32/led/1")   == 0) r.mask = OUT_LED1;
    else if (strcmp(topic, "stm32/led/2")   == 0) r.mask = OUT_LED2;
    else if (strcmp(topic, "stm32/led/3")   == 0) r.mask = OUT_LED3;

    /* External relays on P4 */
    else if (strcmp(topic, "stm32/relay/all") == 0) r.mask = OUT_RELAY_ALL;
    else if (strcmp(topic, "stm32/relay/1")   == 0) r.mask = OUT_RELAY1;
    else if (strcmp(topic, "stm32/relay/2")   == 0) r.mask = OUT_RELAY2;
    else if (strcmp(topic, "stm32/relay/3")   == 0) r.mask = OUT_RELAY3;

    /* else: unknown topic → mask stays 0, no action */
    return r;
}
