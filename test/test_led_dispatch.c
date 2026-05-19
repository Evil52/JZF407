#include "unity/unity.h"
#include "led_dispatch.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* ---------- LED topic routing ---------- */

void test_led1_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
    TEST_ASSERT_EQUAL(0, r.echo);
}

void test_led2_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/2", (unsigned char *)"0", 1);
    TEST_ASSERT_EQUAL_HEX8(0x02, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

void test_led3_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/3", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0x04, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_led_all_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/all", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0x07, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_led_all_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/all", (unsigned char *)"0", 1);
    TEST_ASSERT_EQUAL_HEX8(0x07, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

/* ---------- Payload edge cases ---------- */

void test_payload_other_chars_treated_as_off(void)
{
    /* Per current spec: only "1" turns ON, anything else is OFF */
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", (unsigned char *)"9", 1);
    TEST_ASSERT_EQUAL_HEX8(0x01, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

void test_multi_byte_payload_uses_first_byte(void)
{
    /* "1xxxxx" → ON, ignore rest */
    led_dispatch_t r = led_dispatch_parse("stm32/led/all", (unsigned char *)"1abcd", 5);
    TEST_ASSERT_EQUAL_HEX8(0x07, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_empty_payload_is_noop_for_led(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", (unsigned char *)"", 0);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
    TEST_ASSERT_EQUAL(0, r.echo);
}

void test_null_payload_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/led/1", NULL, 0);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

/* ---------- Echo topic ---------- */

void test_ping_sets_echo_flag(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/ping", (unsigned char *)"hello", 5);
    TEST_ASSERT_EQUAL(1, r.echo);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);  /* no LED change */
}

void test_ping_with_empty_payload_still_echoes(void)
{
    /* Echo doesn't require payload to be parsed */
    led_dispatch_t r = led_dispatch_parse("stm32/ping", NULL, 0);
    TEST_ASSERT_EQUAL(1, r.echo);
}

/* ---------- Unknown / malformed topics ---------- */

void test_unknown_topic_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/random", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
    TEST_ASSERT_EQUAL(0, r.echo);
}

void test_partial_topic_match_is_noop(void)
{
    /* Must be exact match, not prefix */
    led_dispatch_t r = led_dispatch_parse("stm32/led/", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_led_topic_with_trailing_chars(void)
{
    /* "stm32/led/1x" must NOT match "stm32/led/1" */
    led_dispatch_t r = led_dispatch_parse("stm32/led/1x", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

void test_null_topic_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse(NULL, (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
    TEST_ASSERT_EQUAL(0, r.echo);
}

void test_empty_topic_is_noop(void)
{
    led_dispatch_t r = led_dispatch_parse("", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.mask);
}

/* ---------- Relay topics ---------- */

void test_relay1_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/1", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0x10, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_relay2_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/2", (unsigned char *)"0", 1);
    TEST_ASSERT_EQUAL_HEX8(0x20, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

void test_relay3_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/3", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0x40, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_relay_all_on(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/all", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0x70, r.mask);
    TEST_ASSERT_EQUAL(1, r.state);
}

void test_relay_all_off(void)
{
    led_dispatch_t r = led_dispatch_parse("stm32/relay/all", (unsigned char *)"0", 1);
    TEST_ASSERT_EQUAL_HEX8(0x70, r.mask);
    TEST_ASSERT_EQUAL(0, r.state);
}

void test_relay_and_led_masks_dont_overlap(void)
{
    /* LED mask uses low nibble (0x07), relay mask uses high nibble (0x70).
     * This invariant means outputs_apply can dispatch both in one call. */
    led_dispatch_t l = led_dispatch_parse("stm32/led/all",   (unsigned char *)"1", 1);
    led_dispatch_t r = led_dispatch_parse("stm32/relay/all", (unsigned char *)"1", 1);
    TEST_ASSERT_EQUAL_HEX8(0, (l.mask & r.mask));
}

/* ---------- Runner ---------- */
int main(void)
{
    UNITY_BEGIN();
    /* LED */
    RUN_TEST(test_led1_on);
    RUN_TEST(test_led2_off);
    RUN_TEST(test_led3_on);
    RUN_TEST(test_led_all_on);
    RUN_TEST(test_led_all_off);
    /* Payload edge cases */
    RUN_TEST(test_payload_other_chars_treated_as_off);
    RUN_TEST(test_multi_byte_payload_uses_first_byte);
    RUN_TEST(test_empty_payload_is_noop_for_led);
    RUN_TEST(test_null_payload_is_noop);
    /* Echo */
    RUN_TEST(test_ping_sets_echo_flag);
    RUN_TEST(test_ping_with_empty_payload_still_echoes);
    /* Topic safety */
    RUN_TEST(test_unknown_topic_is_noop);
    RUN_TEST(test_partial_topic_match_is_noop);
    RUN_TEST(test_led_topic_with_trailing_chars);
    RUN_TEST(test_null_topic_is_noop);
    RUN_TEST(test_empty_topic_is_noop);
    /* Relays */
    RUN_TEST(test_relay1_on);
    RUN_TEST(test_relay2_off);
    RUN_TEST(test_relay3_on);
    RUN_TEST(test_relay_all_on);
    RUN_TEST(test_relay_all_off);
    RUN_TEST(test_relay_and_led_masks_dont_overlap);
    return UNITY_END();
}
