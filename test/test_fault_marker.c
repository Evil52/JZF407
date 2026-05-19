#include "unity/unity.h"
#include "fault_marker.h"
#include "stm32f4xx.h"
#include <string.h>

extern uint32_t mock_ccmram[8];

/* Internal: fault_marker uses static state. We need to reset it between tests.
 * Since s_captured is static in fault_marker.c, we use a "reset trampoline"
 * declared there only for tests — see TEST_BUILD guard. */
void fault_marker_test_reset(void);  /* defined in fault_marker.c when TEST_BUILD=1 */

#define MAGIC_STACK     0xDEAD1111u
#define MAGIC_MALLOC    0xDEAD2222u
#define MAGIC_SENTINEL  0xA5A5A5A5u

void setUp(void)
{
    mock_reset_all();
    fault_marker_test_reset();
}

void tearDown(void) {}

/* ---------- RCC_CSR classification ---------- */

void test_rcc_power_on(void)
{
    mock_RCC.CSR = RCC_CSR_PORRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_POWER_ON, fault_marker_get()->reason);
}

void test_rcc_software(void)
{
    mock_RCC.CSR = RCC_CSR_SFTRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_SOFTWARE, fault_marker_get()->reason);
}

void test_rcc_iwdg(void)
{
    mock_RCC.CSR = RCC_CSR_IWDGRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_IWDG, fault_marker_get()->reason);
}

void test_rcc_iwdg_takes_priority_over_pin(void)
{
    /* IWDG reset also sets PINRSTF on some MCUs — IWDG must win */
    mock_RCC.CSR = RCC_CSR_IWDGRSTF | RCC_CSR_PINRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_IWDG, fault_marker_get()->reason);
}

void test_rcc_pin(void)
{
    mock_RCC.CSR = RCC_CSR_PINRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_PIN, fault_marker_get()->reason);
}

void test_rcc_brown_out(void)
{
    mock_RCC.CSR = RCC_CSR_BORRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_BROWN_OUT, fault_marker_get()->reason);
}

void test_rcc_csr_cleared_after_capture(void)
{
    mock_RCC.CSR = RCC_CSR_SFTRSTF;
    fault_marker_capture();
    /* RMVF bit must have been written → in real HW would clear all reset flags.
     * We just check the bit was OR'd in. */
    TEST_ASSERT_TRUE(mock_RCC.CSR & RCC_CSR_RMVF);
}

void test_rcc_no_flags_means_unknown(void)
{
    mock_RCC.CSR = 0;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_NONE, fault_marker_get()->reason);
}

/* ---------- CCMRAM marker (software faults) ---------- */

void test_marker_stack_overflow(void)
{
    /* Note: on real STM32 (32-bit pointers) we'd put (uint32_t)"EthIf" in slot 1.
     * On the host (64-bit pointers) we cannot round-trip a pointer through a
     * uint32_t cell, so we just verify the reason and tick are correctly read. */
    mock_RCC.CSR = RCC_CSR_IWDGRSTF;        /* IWDG fired AFTER hook */
    mock_ccmram[0] = MAGIC_STACK;
    mock_ccmram[1] = 0;                     /* would be task name pointer on STM32 */
    mock_ccmram[2] = 123456u;
    mock_ccmram[3] = MAGIC_SENTINEL;

    fault_marker_capture();
    const fault_info_t *fi = fault_marker_get();

    /* Software marker must override RCC reason */
    TEST_ASSERT_EQUAL(RESET_REASON_STACK_OVF, fi->reason);
    TEST_ASSERT_EQUAL_UINT32(123456u, fi->tick_at_fault);
}

void test_marker_malloc_fail(void)
{
    mock_ccmram[0] = MAGIC_MALLOC;
    mock_ccmram[3] = MAGIC_SENTINEL;

    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_MALLOC_FAIL, fault_marker_get()->reason);
    TEST_ASSERT_EQUAL_STRING("malloc", fault_marker_get()->task_name);
}

void test_marker_cleared_after_capture(void)
{
    mock_ccmram[0] = MAGIC_STACK;
    mock_ccmram[3] = MAGIC_SENTINEL;

    fault_marker_capture();

    /* Marker must be cleared so the NEXT reset is not falsely attributed */
    TEST_ASSERT_EQUAL_UINT32(0, mock_ccmram[0]);
    TEST_ASSERT_EQUAL_UINT32(0, mock_ccmram[3]);
}

void test_marker_ignored_without_sentinel(void)
{
    /* Random garbage in CCMRAM after power-on should NOT be treated as marker */
    mock_ccmram[0] = MAGIC_STACK;
    mock_ccmram[3] = 0xDEADBEEFu;  /* not the sentinel */
    mock_RCC.CSR  = RCC_CSR_PORRSTF;

    fault_marker_capture();
    TEST_ASSERT_EQUAL(RESET_REASON_POWER_ON, fault_marker_get()->reason);
}

void test_capture_is_idempotent(void)
{
    mock_RCC.CSR = RCC_CSR_SFTRSTF;
    fault_marker_capture();
    reset_reason_t first = fault_marker_get()->reason;

    /* Second call must not re-read RCC (which we now wipe) */
    mock_RCC.CSR = RCC_CSR_IWDGRSTF;
    fault_marker_capture();
    TEST_ASSERT_EQUAL(first, fault_marker_get()->reason);
}

/* ---------- reason_str ---------- */

void test_reason_str_known(void)
{
    TEST_ASSERT_EQUAL_STRING("iwdg_timeout",  fault_marker_reason_str(RESET_REASON_IWDG));
    TEST_ASSERT_EQUAL_STRING("software",      fault_marker_reason_str(RESET_REASON_SOFTWARE));
    TEST_ASSERT_EQUAL_STRING("stack_overflow",fault_marker_reason_str(RESET_REASON_STACK_OVF));
    TEST_ASSERT_EQUAL_STRING("malloc_failed", fault_marker_reason_str(RESET_REASON_MALLOC_FAIL));
}

void test_reason_str_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown", fault_marker_reason_str((reset_reason_t)999));
}

/* ---------- runner ---------- */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rcc_power_on);
    RUN_TEST(test_rcc_software);
    RUN_TEST(test_rcc_iwdg);
    RUN_TEST(test_rcc_iwdg_takes_priority_over_pin);
    RUN_TEST(test_rcc_pin);
    RUN_TEST(test_rcc_brown_out);
    RUN_TEST(test_rcc_csr_cleared_after_capture);
    RUN_TEST(test_rcc_no_flags_means_unknown);
    RUN_TEST(test_marker_stack_overflow);
    RUN_TEST(test_marker_malloc_fail);
    RUN_TEST(test_marker_cleared_after_capture);
    RUN_TEST(test_marker_ignored_without_sentinel);
    RUN_TEST(test_capture_is_idempotent);
    RUN_TEST(test_reason_str_known);
    RUN_TEST(test_reason_str_unknown);
    return UNITY_END();
}
