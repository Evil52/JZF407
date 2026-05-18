/* Minimal STM32F4 mock for host-side unit tests. */
#ifndef MOCK_STM32F4XX_H
#define MOCK_STM32F4XX_H

#include <stdint.h>

/* RCC mock — only CSR with the flags we read */
typedef struct {
    volatile uint32_t CSR;
} mock_RCC_TypeDef;

extern mock_RCC_TypeDef mock_RCC;
#define RCC (&mock_RCC)

#define RCC_CSR_PORRSTF   (1u << 27)
#define RCC_CSR_PINRSTF   (1u << 26)
#define RCC_CSR_SFTRSTF   (1u << 28)
#define RCC_CSR_IWDGRSTF  (1u << 29)
#define RCC_CSR_WWDGRSTF  (1u << 30)
#define RCC_CSR_LPWRRSTF  (1u << 31)
#define RCC_CSR_BORRSTF   (1u << 25)
#define RCC_CSR_RMVF      (1u << 24)

/* Fake CCMRAM region used by fault_marker via -DFAULT_MARKER_ADDR=mock_ccmram */
extern uint32_t mock_ccmram[8];

/* Helpers for tests */
void mock_reset_all(void);

#endif
