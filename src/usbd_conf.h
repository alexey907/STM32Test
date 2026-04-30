#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Forward-declare FreeRTOS allocator (avoid pulling FreeRTOS.h into framework USB core) */
extern void *pvPortMalloc(size_t xWantedSize);
extern void vPortFree(void *pv);
#define USBD_MAX_NUM_INTERFACES               2U
#define USBD_MAX_NUM_CONFIGURATION            1U
#define USBD_MAX_STR_DESC_SIZ                 0x100U
#define USBD_SUPPORT_USER_STRING_DESC         0U
#define USBD_SELF_POWERED                     1U
#define USBD_DEBUG_LEVEL                      0U

/* Memory management macros — use FreeRTOS heap (standard malloc is 0 bytes on this target) */
#define USBD_malloc               pvPortMalloc
#define USBD_free                 vPortFree
#define USBD_memset               memset
#define USBD_memcpy               memcpy

/* DEBUG macros (disabled) */
#define USBD_UsrLog(...)  do {} while (0)
#define USBD_ErrLog(...)  do {} while (0)
#define USBD_DbgLog(...)  do {} while (0)

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_H */