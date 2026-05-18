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
#include "lwip/ip_addr.h"
#include "cmsis_os.h"
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

static mqtt_client_t *s_client;
static volatile uint8_t s_connected = 0;

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
    if (status == MQTT_CONNECT_ACCEPTED) {
        s_connected = 1;

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
        s_connected = 0;
    }
}

/* ── connect request (must run on tcpip thread) ──────── */
static struct mqtt_connect_client_info_t s_ci;

static void do_connect(void *arg)
{
    (void)arg;
    ip_addr_t broker;
    ipaddr_aton(MQTT_BROKER_IP, &broker);

    memset(&s_ci, 0, sizeof(s_ci));
    s_ci.client_id   = MQTT_CLIENT_ID;
    s_ci.keep_alive  = 60;
    /* no LWT for now — keep it simple */

    mqtt_client_connect(s_client, &broker, MQTT_BROKER_PORT,
                        on_connection, NULL, &s_ci);
}

/* ── FreeRTOS task ───────────────────────────────────── */
void mqtt_app_task(void *argument)
{
    (void)argument;

    /* Wait for link up */
    while (!netif_is_link_up(&gnetif)) {
        osDelay(200);
    }
    osDelay(1000);

    s_client = mqtt_client_new();
    if (!s_client) Error_Handler();

    /* Initial connect */
    tcpip_callback(do_connect, NULL);

    /* Monitor connection — reconnect only if truly disconnected */
    for (;;) {
        osDelay(2000);
        if (!mqtt_client_is_connected(s_client) && !s_connected) {
            tcpip_callback(do_connect, NULL);
        }
    }
}
