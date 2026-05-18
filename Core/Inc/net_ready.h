#ifndef __NET_READY_H__
#define __NET_READY_H__

#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Semaphore signalled by defaultTask after MX_LWIP_Init() returns.
 * Other tasks (e.g. mqtt_app_task) wait on it before touching gnetif. */
extern osSemaphoreId_t netReadySem;

void net_ready_init(void);    /* call before tasks start using it */
void net_ready_signal(void);  /* called by defaultTask after MX_LWIP_Init */
void net_ready_wait(void);    /* called by consumers; blocks forever then re-releases */

#ifdef __cplusplus
}
#endif

#endif /* __NET_READY_H__ */
