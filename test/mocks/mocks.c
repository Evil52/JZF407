/* Mock state definitions */
#include "stm32f4xx.h"
#include <string.h>

mock_RCC_TypeDef mock_RCC;

/* Fake CCMRAM region used by fault_marker — actual address overridden by
 * MARKER_ADDR redirect via -D in CMake */
uint32_t mock_ccmram[8] = { 0 };

void mock_reset_all(void)
{
    mock_RCC.CSR = 0;
    memset(mock_ccmram, 0, sizeof(mock_ccmram));
}
