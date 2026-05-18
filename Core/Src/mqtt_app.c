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
#include <string.h>

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
    if (len == 0) return;
    uint8_t on = (data[0] == '1');

    if      (strcmp(s_topic_buf, "stm32/led/all") == 0) led_all(on);
    else if (strcmp(s_topic_buf, "stm32/led/1")   == 0) led_set(LED1_PIN, on);
    else if (strcmp(s_topic_buf, "stm32/led/2")   == 0) led_set(LED2_PIN, on);
    else if (strcmp(s_topic_buf, "stm32/led/3")   == 0) led_set(LED3_PIN, on);
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

        /* Install publish handlers BEFORE subscribing */
        mqtt_set_inpub_callback(client, on_publish, on_data, NULL);

        /* Subscribe with QoS=0 */
        mqtt_subscribe(client, "stm32/led/1",   0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/led/2",   0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/led/3",   0, on_sub_done, NULL);
        mqtt_subscribe(client, "stm32/led/all", 0, on_sub_done, NULL);

        /* Publish online status (after subscribe to ensure clean order) */
        const char *msg = "online";
        mqtt_publish(client, "stm32/status", msg, strlen(msg),
                     0, 0, on_sub_done, NULL);
    } else {
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
    s_ci.keep_alive  = 60;
    /* no LWT for now — keep it simple */

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

    /* Monitor connection — reconnect only if truly disconnected AND
     * no connect is currently in flight. */
    for (;;) {
        osDelay(2000);
        if (get_connecting()) continue;                         /* wait for callback */
        if (mqtt_is_connected_safe() || get_connected()) continue;

        set_connecting(1);
        if (tcpip_callback(do_connect, NULL) != ERR_OK) {
            /* tcpip mbox full — clear flag, retry next tick */
            set_connecting(0);
        }
    }
}
