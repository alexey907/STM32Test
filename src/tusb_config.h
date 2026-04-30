#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_MCU               OPT_MCU_STM32F1
#define CFG_TUSB_OS                OPT_OS_FREERTOS

// Enable device mode on root port 0 (STM32F1 has only one USB port)
#define CFG_TUSB_RHPORT0_MODE      OPT_MODE_DEVICE

// We want exactly 1 CDC (Virtual Serial) port
#define CFG_TUD_CDC                1

// TinyUSB allocates FIFOs (buffers) automatically based on these sizes
#define CFG_TUD_CDC_RX_BUFSIZE     64
#define CFG_TUD_CDC_TX_BUFSIZE     64

#endif /* _TUSB_CONFIG_H_ */