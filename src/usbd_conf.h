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

/* Forward-declare custom static allocator (avoids FreeRTOS heap leak on USB replug) */
extern void* usb_malloc(size_t size);
extern void usb_free(void* ptr);
#define USBD_MAX_NUM_INTERFACES               2U
#define USBD_MAX_NUM_CONFIGURATION            1U
#define USBD_MAX_STR_DESC_SIZ                 0x100U
#define USBD_SUPPORT_USER_STRING_DESC         0U
#define USBD_SELF_POWERED                     1U
#define USBD_DEBUG_LEVEL                      0U

/* Memory management macros — static allocation, immune to heap exhaustion on replug */
#define USBD_malloc               usb_malloc
#define USBD_free                 usb_free
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