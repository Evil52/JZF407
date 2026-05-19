/*
 * MQTT LED control over Ethernet (LwIP + FreeRTOS)
 *
 * Board:   STM32F407VETx
 * PHY:     DP83848 (RMII)
 * LEDs:    PE13, PE14, PE15  (active-LOW)
 *
 * All LwIP/MQTT API calls run on the tcpip thread via tcpip_callback().
 *
 * Topics subscribed:
 *   stm32/led/1   payload "1"/"0"  -> PE13
 *   stm32/led/2   payload "1"/"0"  -> PE14
 *   stm32/led/3   payload "1"/"0"  -> PE15
 *   stm32/led/all payload "1"/"0"  -> all three LEDs
 *
 * Topic published on connect: stm32/status "online"
 * Last-will on disconnect:    stm32/status "offline"
 */

#include "mqtt_app.h"
#include "main.h"
#include "lwip.h"
#include "lwip/apps/mqtt.h"
#include "lwip/tcpip.h"
#include "lwip/priv/tcpip_priv.h"   /* LOCK_TCPIP_CORE / UNLOCK_TCPIP_CORE */
#include "lwip/ip_addr.h"
#include "cmsis_os.h"
#include "net_ready.h"
#include "fault_marker.h"
#include "led_dispatch.h"
#include "outputs.h"
#include <string.h>
#include <stdio.h>

/* Fail-safe configuration */
#define GRACE_PERIOD_MS  3000U   /* ignore incoming msgs (incl. retained) for 3s after connect */
#define HEARTBEAT_MS    10000U   /* publish stm32/heartbeat every 10s — keeps TCP alive */

/* LED3 visual heartbeat (independent of MQTT) */
#define HB_LED_PERIOD_MS  7000U  /* every 7 seconds */
#define HB_LED_ON_MS       100U  /* LED on for 100 ms */

/* ── user config ─────────────────────────────────────── */
#define MQTT_BROKER_IP    "192.168.137.1"
#define MQTT_BROKER_PORT  1883
#define MQTT_CLIENT_ID    "stm32f407"
/* ─────────────────────────────────────────────────────── */

#define LED1_PIN   GPIO_PIN_13
#define LED2_PIN   GPIO_PIN_14
#define LED3_PIN   GPIO_PIN_15
#define LED_PORT   GPIOE

extern struct netif gnetif;

/* Shared between mqttTask (read) and tcpip_thread (write).
 * volatile + explicit DMB barriers; single-byte access is atomic on Cortex-M4. */
static mqtt_client_t * volatile s_client;
static volatile uint8_t          s_connected  = 0;
static volatile uint8_t          s_connecting = 0;  /* in-flight connect request */

static inline void set_connected(uint8_t v)
{
    s_connected = v;
    __DMB();
}

static inline uint8_t get_connected(void)
{
    __DMB();
    return s_connected;
}

static inline void set_connecting(uint8_t v)
{
    s_connecting = v;
    __DMB();
}

static inline uint8_t get_connecting(void)
{
    __DMB();
    return s_connecting;
}

/* LEDs are active-LOW */
static inline void led_set(uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(LED_PORT, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void led_all(uint8_t on)
{
    led_set(LED1_PIN, on);
    led_set(LED2_PIN, on);
    led_set(LED3_PIN, on);
}

/* Forward decl */
static void on_sub_done(void *arg, err_t err);

/* Grace period: after a fresh connect, ignore the first GRACE_PERIOD_MS of
 * incoming messages. Brokers replay retained messages immediately after
 * SUBSCRIBE — without this filter, the device would re-apply the last known
 * "ON" command from days ago and re-energise relays right after a power dip. */
static volatile uint32_t s_grace_until_ms = 0;

static inline uint8_t in_grace_period(void)
{
    int32_t remaining = (int32_t)(s_grace_until_ms - HAL_GetTick());
    return (remaining > 0) ? 1 : 0;
}

/* ── incoming publish (called from tcpip thread) ─────── */
static char s_topic_buf[64];

static void on_publish(void *arg, const char *topic, u32_t tot_len)
{
    (void)arg; (void)tot_len;
    strncpy(s_topic_buf, topic, sizeof(s_topic_buf) - 1);
    s_topic_buf[sizeof(s_topic_buf) - 1] = '\0';
}

static void on_data(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    (void)arg; (void)flags;

    led_dispatch_t d = led_dispatch_parse(s_topic_buf, data, len);

    /* Echo always honoured — used for load-test RTT measurement, no side effect */
    if (d.echo) {
        if (s_client) {
            mqtt_publish(s_client, "stm32/pong", data, len,
                         0, 0, on_sub_done, NULL);
        }
        return;
    }

    /* During grace period, drop output commands. This prevents retained
     * "ON" messages from auto-energising relays right after a reconnect. */
    if (in_grace_period()) {
        return;
    }

    if (d.mask) outputs_apply(d.mask, d.state);
}

static void on_sub_done(void *arg, err_t err)
{
    (void)arg; (void)err;
}

/* ── connection callback (tcpip thread) ──────────────── */
static void on_connection(mqtt_client_t *client, void *arg,
                          mqtt_connection_status_t status)
{
    (void)arg;
    set_connecting(0);  /* connect attempt finished (success or fail) */

    if (status == MQTT_CONNECT_ACCEPTED) {
        set_connected(1);

        /* Start grace period — any retained command arriving in the next
         * GRACE_PERIOD_MS is dropped by on_data(). Brokers replay retained
         * messages immediately after SUBSCRIBE; this protects against an
         * old "ON" command re-energising relays after a power dip / reconnect. */
        s_grace_until_ms = HAL_GetTick() + GRACE_PERIOD_MS;
        __DMB();

        /* Install publish handlers BEFORE subscribing */
        mqtt_set_inpub_callback(client, on_publish, on_data, NULL);

        /* Subscribe with QoS=0. Total in-flight = 5 subs + 2 publish = 7.
         * MQTT_REQ_MAX_IN_FLIGHT in lwipopts.h must be >= 7. */
        mqtt_subscribe(client, "stm32/led/1",   0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/led/2",   0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/led/all", 0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/relay",   0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/ping",    0, on_sub_done, NULL);  /* load-test echo */

        /* Publish online status — retained so dashboards see it on (re)subscribe.
         * Combined with LWT "offline", this gives reliable presence tracking. */
        const char *msg = "online";
        mqtt_publish(client, "stm32/status", msg, strlen(msg),
                     0, /*retain=*/1, on_sub_done, NULL);

        /* Publish reset reason as a plain string (no snprintf — runs on
         * tcpip_thread which has only 1 KB of stack). */
        const fault_info_t *fi = fault_marker_get();
        const char *reason = fault_marker_reason_str(fi->reason);
        mqtt_publish(client, "stm32/diag", reason, strlen(reason),
                     0, /*retain=*/1, on_sub_done, NULL);
    } else {
        /* Fail-LAST policy: outputs keep their current state across MQTT
         * outages. The shadow in outputs.c plus the RTC backup register
         * are the source of truth, not the broker's retained messages. */
        set_connected(0);
    }
}

/* ── connect request (must run on tcpip thread) ──────── */
static struct mqtt_connect_client_info_t s_ci;

static void do_connect(void *arg)
{
    (void)arg;
    if (!s_client) { set_connecting(0); return; }

    ip_addr_t broker;
    if (!ipaddr_aton(MQTT_BROKER_IP, &broker)) {
        /* Invalid IP literal — cannot recover, just bail out */
        set_connecting(0);
        return;
    }

    memset(&s_ci, 0, sizeof(s_ci));
    s_ci.client_id   = MQTT_CLIENT_ID;
    /* keep_alive=120s: long timeout, our 10s heartbeat keeps TCP active
     * regardless of lwIP MQTT PINGREQ behaviour. Brokers see PUBLISH every
     * 10s → no idle disconnect. */
    s_ci.keep_alive  = 120;
    /* Last Will: broker publishes "offline" if we vanish without DISCONNECT.
     * Retained so any new subscriber sees the current state immediately. */
    s_ci.will_topic  = "stm32/status";
    s_ci.will_msg    = "offline";
    s_ci.will_qos    = 0;
    s_ci.will_retain = 1;

    err_t r = mqtt_client_connect(s_client, &broker, MQTT_BROKER_PORT,
                                  on_connection, NULL, &s_ci);
    if (r != ERR_OK) {
        /* Connect dispatch failed (e.g. ERR_ISCONN). Clear flag so the
         * monitor loop can retry later. */
        set_connecting(0);
    }
}

/* Helper: check connection state safely (locks LwIP core).
 * mqtt_client_is_connected reads internal state managed by tcpip_thread. */
static uint8_t mqtt_is_connected_safe(void)
{
    if (!s_client) return 0;
    LOCK_TCPIP_CORE();
    uint8_t r = mqtt_client_is_connected(s_client) ? 1 : 0;
    UNLOCK_TCPIP_CORE();
    return r;
}

/* Heartbeat: runs on tcpip thread via tcpip_callback (the only safe way
 * to call lwIP API from another FreeRTOS task without LWIP_TCPIP_CORE_LOCKING). */
static void do_heartbeat(void *arg)
{
    (void)arg;
    if (!s_client) return;
    if (mqtt_client_is_connected(s_client)) {
        mqtt_publish(s_client, "stm32/heartbeat", "1", 1,
                     0, 0, on_sub_done, NULL);
    }
}

/* ── FreeRTOS task ───────────────────────────────────── */
void mqtt_app_task(void *argument)
{
    (void)argument;

    /* Wait until defaultTask has finished MX_LWIP_Init() — gnetif is now valid */
    net_ready_wait();

    /* Wait for link up */
    while (!netif_is_link_up(&gnetif)) {
        osDelay(200);
    }
    osDelay(1000);

    mqtt_client_t *c = mqtt_client_new();
    if (!c) Error_Handler();
    s_client = c;   /* publish pointer after fully constructed */
    __DMB();

    /* Initial connect */
    set_connecting(1);
    if (tcpip_callback(do_connect, NULL) != ERR_OK) {
        set_connecting(0);
    }

    /* Monitor connection + heartbeat publish + heartbeat LED.
     * Tick at 100 ms — fine enough for crisp 100 ms LED pulse, low CPU cost. */
    uint32_t last_mqtt_hb_ms = HAL_GetTick();
    uint32_t last_led_hb_ms  = HAL_GetTick();
    uint8_t  led_on          = 0;

    for (;;) {
        osDelay(100);

        /* --- LED3 visual heartbeat: 100 ms ON every 7 s --- */
        uint32_t now = HAL_GetTick();
        if (!led_on && (now - last_led_hb_ms) >= HB_LED_PERIOD_MS) {
            heartbeat_led_on();
            led_on = 1;
            last_led_hb_ms = now;
        } else if (led_on && (now - last_led_hb_ms) >= HB_LED_ON_MS) {
            heartbeat_led_off();
            led_on = 0;
            /* keep last_led_hb_ms as the ON timestamp; the next pulse will fire
             * HB_LED_PERIOD_MS later from start-of-ON (period, not gap). */
        }

        /* --- MQTT heartbeat publish --- */
        if (get_connected() &&
            (now - last_mqtt_hb_ms) >= HEARTBEAT_MS) {
            tcpip_callback(do_heartbeat, NULL);
            last_mqtt_hb_ms = now;
        }

        if (get_connecting()) continue;
        if (mqtt_is_connected_safe() || get_connected()) continue;

        set_connecting(1);
        if (tcpip_callback(do_connect, NULL) != ERR_OK) {
            set_connecting(0);
        }
    }
}
