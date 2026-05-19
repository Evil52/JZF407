#include "unity/unity.h"
#include "led_dispatch.h"
#include "outputs.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* ---------- LED topic routing ---------- */

void test_led1_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(OUT_LED1, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
    TEST_ASSERT_EQUAL(0, r.echo);
}

void test_led2_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/2", (unsigned char *)"0", 1);
    TEST_ASSERT_EQUAL_HEX8(OUT_LED2, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

void test_led3_not_exposed(void)
{
    /* LED3 (PE15) is reserved for heartbeat — must NOT be MQTT-controllable */
    led_dispatch_t r = led_dispatch_parse("stm32/led/3", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_led_all_covers_led1_and_led2_only(void)
{
    /* "all" must NOT include LED3 (heartbeat) */
    led_dispatch_t r = led_dispatch_parse("stm32/led/all", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(OUT_LED1 | OUT_LED2, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_relay_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(OUT_RELAY, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_relay_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay", (unsigned char *)"0", 1);
    TEST_ASSERT_EQUAL_HEX8(OUT_RELAY, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

/* ---------- Old relay paths must be gone ---------- */

void test_old_relay_1_no_longer_routes(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/1", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_old_relay_all_no_longer_routes(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/all", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

/* ---------- Payload edge cases ---------- */

void test_payload_other_chars_treated_as_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", (unsigned char *)"9", 1);
    TEST_ASSERT_EQUAL_HEX8(OUT_LED1, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

void test_empty_payload_is_noop_for_led(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", (unsigned char *)"", 0);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_null_payload_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", NULL, 0);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

/* ---------- Echo ---------- */

void test_ping_sets_echo_flag(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/ping", (unsigned char *)"hello", 5);
    TEST_ASSERT_EQUAL(1, r.echo);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_ping_with_empty_payload_still_echoes(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/ping", NULL, 0);
    TEST_ASSERT_EQUAL(1, r.echo);
}

/* ---------- Unknown / malformed ---------- */

void test_unknown_topic_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/random", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_partial_topic_match_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_null_topic_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse(NULL, (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

/* ---------- Bit layout invariants ---------- */

void test_masks_dont_overlap(void)
{
    /* The 3 managed outputs must use distinct bits */
    TEST_ASSERT_EQUAL_HEX8(0, (OUT_LED1  & OUT_LED2));
    TEST_ASSERT_EQUAL_HEX8(0, (OUT_LED1  & OUT_RELAY));
    TEST_ASSERT_EQUAL_HEX8(0, (OUT_LED2  & OUT_RELAY));
    TEST_ASSERT_EQUAL_HEX8(OUT_ALL, (OUT_LED1 | OUT_LED2 | OUT_RELAY));
}

/* ---------- Runner ---------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_led1_on);
    RUN_TEST(test_led2_off);
    RUN_TEST(test_led3_not_exposed);
    RUN_TEST(test_led_all_covers_led1_and_led2_only);
    RUN_TEST(test_relay_on);
    RUN_TEST(test_relay_off);
    RUN_TEST(test_old_relay_1_no_longer_routes);
    RUN_TEST(test_old_relay_all_no_longer_routes);
    RUN_TEST(test_payload_other_chars_treated_as_off);
    RUN_TEST(test_empty_payload_is_noop_for_led);
    RUN_TEST(test_null_payload_is_noop);
    RUN_TEST(test_ping_sets_echo_flag);
    RUN_TEST(test_ping_with_empty_payload_still_echoes);
    RUN_TEST(test_unknown_topic_is_noop);
    RUN_TEST(test_partial_topic_match_is_noop);
    RUN_TEST(test_null_topic_is_noop);
    RUN_TEST(test_masks_dont_overlap);
    return UNITY_END();
}
