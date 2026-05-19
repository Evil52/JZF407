#include "led_dispatch.h"
#include "outputs.h"
#include <string.h>

led_dispatch_t led_dispatch_parse(const char *topic,
                                  const unsigned char *payload,
                                  size_t payload_len)
{
    led_dispatch_t r = { 0, 0, 0 };

    if (!topic) return r;

    /* Echo topic — no output change */
    if (strcmp(topic, "stm32/ping") == 0) {
        r.echo = 1;
        return r;
    }

    if (payload_len == 0 || !payload) return r;
    r.state = (payload[0] == '1') ? 1 : 0;

    /* MQTT-managed outputs.
     * LED3 (PE15) is reserved for heartbeat — not exposed via MQTT. */
    if      (strcmp(topic, "stm32/led/1")   == 0) r.mask = OUT_LED1;
    else if (strcmp(topic, "stm32/led/2")   == 0) r.mask = OUT_LED2;
    else if (strcmp(topic, "stm32/led/all") == 0) r.mask = OUT_LED1 | OUT_LED2;
    else if (strcmp(topic, "stm32/relay")   == 0) r.mask = OUT_RELAY;

    return r;
}
