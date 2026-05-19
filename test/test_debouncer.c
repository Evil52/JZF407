#include "unity/unity.h"
#include "debouncer.h"

static debouncer_t d;

void setUp(void)    { debouncer_init(&d, 4); }
void tearDown(void) {}

/* Helper: feed raw value n times */
static void feed_n(uint8_t raw, int n)
{
    for (int i = 0; i < n; i++) debouncer_feed(&d, raw);
}

/* ---------- Initial state ---------- */

void test_initial_state_is_released(void)
{
    TEST_ASSERT_FALSE(debouncer_take_press(&d));
    TEST_ASSERT_FALSE(debouncer_take_release(&d));
}

/* ---------- Press detection ---------- */

void test_press_after_4_low_samples(void)
{
    feed_n(0, 3);                          /* 3 lows — not enough */
    TEST_ASSERT_FALSE(debouncer_take_press(&d));
    feed_n(0, 1);                          /* 4th low — debounce satisfied */
    TEST_ASSERT_TRUE(debouncer_take_press(&d));
}

void test_press_edge_consumed_once(void)
{
    feed_n(0, 4);
    TEST_ASSERT_TRUE(debouncer_take_press(&d));
    TEST_ASSERT_FALSE(debouncer_take_press(&d));  /* second call: nothing */
}

void test_holding_button_doesnt_repeat_press(void)
{
    feed_n(0, 4);
    TEST_ASSERT_TRUE(debouncer_take_press(&d));
    feed_n(0, 100);                        /* keep button held */
    TEST_ASSERT_FALSE(debouncer_take_press(&d));
}

/* ---------- Release detection ---------- */

void test_release_after_4_high_samples(void)
{
    feed_n(0, 4);
    (void)debouncer_take_press(&d);

    feed_n(1, 3);
    TEST_ASSERT_FALSE(debouncer_take_release(&d));
    feed_n(1, 1);
    TEST_ASSERT_TRUE(debouncer_take_release(&d));
}

/* ---------- Bounce rejection ---------- */

void test_short_glitch_does_not_trigger_press(void)
{
    feed_n(0, 2);   /* 2 lows then bounce back */
    feed_n(1, 2);
    feed_n(0, 2);
    feed_n(1, 5);
    /* No sustained press happened */
    TEST_ASSERT_FALSE(debouncer_take_press(&d));
}

void test_bouncing_then_settling_low_eventually_presses(void)
{
    /* Bouncy press: lots of HL HL HL, then settle LOW */
    for (int i = 0; i < 5; i++) {
        debouncer_feed(&d, 0);
        debouncer_feed(&d, 1);
    }
    /* No press yet — never had 4 in a row */
    TEST_ASSERT_FALSE(debouncer_take_press(&d));

    /* Now stable */
    feed_n(0, 4);
    TEST_ASSERT_TRUE(debouncer_take_press(&d));
}

/* ---------- Full press-release cycle ---------- */

void test_full_cycle(void)
{
    feed_n(0, 4); TEST_ASSERT_TRUE(debouncer_take_press(&d));
    feed_n(1, 4); TEST_ASSERT_TRUE(debouncer_take_release(&d));
    feed_n(0, 4); TEST_ASSERT_TRUE(debouncer_take_press(&d));
    feed_n(1, 4); TEST_ASSERT_TRUE(debouncer_take_release(&d));
}

/* ---------- Threshold range ---------- */

void test_threshold_clamped_to_min_1(void)
{
    debouncer_init(&d, 0);
    feed_n(0, 1);
    TEST_ASSERT_TRUE(debouncer_take_press(&d));
}

void test_threshold_clamped_to_max_8(void)
{
    debouncer_init(&d, 99);
    feed_n(0, 7);
    TEST_ASSERT_FALSE(debouncer_take_press(&d));
    feed_n(0, 1);
    TEST_ASSERT_TRUE(debouncer_take_press(&d));
}

/* ---------- NULL safety ---------- */

void test_null_pointer_safe(void)
{
    debouncer_feed(NULL, 0);
    TEST_ASSERT_FALSE(debouncer_take_press(NULL));
    TEST_ASSERT_FALSE(debouncer_take_release(NULL));
}

/* ---------- Runner ---------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_released);
    RUN_TEST(test_press_after_4_low_samples);
    RUN_TEST(test_press_edge_consumed_once);
    RUN_TEST(test_holding_button_doesnt_repeat_press);
    RUN_TEST(test_release_after_4_high_samples);
    RUN_TEST(test_short_glitch_does_not_trigger_press);
    RUN_TEST(test_bouncing_then_settling_low_eventually_presses);
    RUN_TEST(test_full_cycle);
    RUN_TEST(test_threshold_clamped_to_min_1);
    RUN_TEST(test_threshold_clamped_to_max_8);
    RUN_TEST(test_null_pointer_safe);
    return UNITY_END();
}
