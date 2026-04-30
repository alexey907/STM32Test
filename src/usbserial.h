#ifndef USBSERIAL_H
#define USBSERIAL_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

/* Now include the USB stack (needs usbd_conf defines above) */
#include "usbd_core.h"
#include "usbd_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* USB descriptor callbacks */
uint8_t *GetDeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *GetLangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *GetManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *GetProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *GetSerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *GetConfigurationStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
uint8_t *GetInterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

#ifdef __cplusplus
}
#endif

/**
 * @brief Self-contained USB CDC Serial class for STM32F103 BluePill.
 * 
 * Handles all hardware config (PA12 pull-up, PCD init, USB stack init).
 * Call begin() once, then use println() to send log output to USB host.
 */
class UsbSerial {
public:
    UsbSerial();
    
    /**
     * @brief Initialize USB peripheral and start CDC device.
     *        Must be called once before using println().
     */
    void begin();
    
    /**
     * @brief Send a null-terminated string followed by "\r\n".
     *        Non-blocking: if USB is not ready or a previous transfer
     *        is in progress, the message is silently dropped.
     */
    void println(const char* str);
    
    /**
     * @brief Send just "\r\n" (empty line).
     */
    void println();
    
    /**
     * @brief Returns true when USB host has enumerated and configured
     *        the CDC device.
     */
    bool ready() const { return _connected; }

private:
    USBD_HandleTypeDef   _usbd;
    USBD_CDC_ItfTypeDef  _cdc_if;
    USBD_DescriptorsTypeDef _desc;
    
    uint8_t  _tx_buf[64];
    bool     _connected;
    
    /* Static C-callbacks required by STM32 middleware */
    static int8_t _InitCb(void);
    static int8_t _DeInitCb(void);
    static int8_t _ControlCb(uint8_t cmd, uint8_t* pbuf, uint16_t length);
    static int8_t _ReceiveCb(uint8_t* buf, uint32_t* len);
    
    /* Singleton for callback access */
    static UsbSerial* _instance;
};

#endif // USBSERIAL_H