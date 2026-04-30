#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USBD_MAX_NUM_INTERFACES               1U
#define USBD_MAX_NUM_CONFIGURATION            1U
#define USBD_MAX_STR_DESC_SIZ                 0x100U
#define USBD_SUPPORT_USER_STRING_DESC         0U
#define USBD_SELF_POWERED                     1U
#define USBD_DEBUG_LEVEL                      0U

/* Memory management macros */
#define USBD_malloc               malloc
#define USBD_free                 free
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