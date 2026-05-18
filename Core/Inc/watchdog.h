#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Start IWDG with ~8s timeout (LSI=32 kHz, /256, reload=1000).
 * Once started, IWDG cannot be stopped except by reset. */
void watchdog_start(void);

/* Refresh (kick) the watchdog. Must be called periodically — at least
 * once every ~7s to be safe. */
void watchdog_refresh(void);

/* FreeRTOS task entry — refreshes the watchdog every 1.5s.
 * Runs at osPriorityLow so that any high-priority task starvation
 * (which is itself a bug) will cause the watchdog to fire. */
void watchdog_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __WATCHDOG_H__ */
