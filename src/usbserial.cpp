#include "usbserial.h"

/* ==================================================================
 * USB Descriptor Tables (ROM)
 * ================================================================== */

/* USB Standard Device Descriptor */
static const uint8_t _dev_desc[18] = {
    0x12,                       // bLength
    0x01,                       // bDescriptorType: Device
    0x00, 0x02,                 // bcdUSB 2.0
    0x02,                       // bDeviceClass: CDC
    0x00,                       // bDeviceSubClass
    0x00,                       // bDeviceProtocol
    0x40,                       // bMaxPacketSize0: 64
    0x83, 0x04,                 // idVendor: 0x0483 (STMicro)
    0x40, 0x57,                 // idProduct: 0x5740 (Virtual COM Port)
    0x00, 0x02,                 // bcdDevice: 2.00
    0x01,                       // iManufacturer
    0x02,                       // iProduct
    0x03,                       // iSerialNumber
    0x01                        // bNumConfigurations
};

/* String Descriptors ------------------------------------------------- */

static const uint8_t _lang_desc[] = {
    0x04,                       // bLength
    0x03,                       // bDescriptorType: String
    0x09, 0x04                  // LangID: US English
};

static const uint8_t _mfc_desc[] = {
    12,                         // bLength (5 chars * 2 + 2 header = 12)
    0x03,                       // bDescriptorType: String
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0
};

static const uint8_t _prod_desc[] = {
    22,                         // bLength (10 chars * 2 + 2 header = 22)
    0x03,                       // bDescriptorType: String
    'U', 0, 'S', 0, 'B', 0, ' ', 0, 'S', 0, 'e', 0, 'r', 0, 'i', 0, 'a', 0, 'l', 0
};

static const uint8_t _serial_desc[] = {
    10,                         // bLength (4 chars * 2 + 2 header = 10)
    0x03,                       // bDescriptorType: String
    '0', 0, '0', 0, '0', 0, '1', 0
};

static const uint8_t _cfg_str_desc[] = {
    22,                         // bLength (10 chars * 2 + 2 header = 22)
    0x03,                       // bDescriptorType: String
    'C', 0, 'D', 0, 'C', 0, ' ', 0, 'C', 0, 'o', 0, 'n', 0, 'f', 0, 'i', 0, 'g', 0
};

static const uint8_t _if_str_desc[] = {
    28,                         // bLength (13 chars * 2 + 2 header = 28)
    0x03,                       // bDescriptorType: String
    'C', 0, 'D', 0, 'C', 0, ' ', 0,
    'I', 0, 'n', 0, 't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0
};

/* ==================================================================
 * Descriptor callback functions (C linkage)
 * ================================================================== */

extern "C" {

uint8_t *GetDeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_dev_desc);
    return (uint8_t*)_dev_desc;
}

uint8_t *GetLangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_lang_desc);
    return (uint8_t*)_lang_desc;
}

uint8_t *GetManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_mfc_desc);
    return (uint8_t*)_mfc_desc;
}

uint8_t *GetProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_prod_desc);
    return (uint8_t*)_prod_desc;
}

uint8_t *GetSerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_serial_desc);
    return (uint8_t*)_serial_desc;
}

uint8_t *GetConfigurationStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_cfg_str_desc);
    return (uint8_t*)_cfg_str_desc;
}

uint8_t *GetInterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(_if_str_desc);
    return (uint8_t*)_if_str_desc;
}

} // extern "C"

/* ==================================================================
 * Static C callback implementations for CDC interface
 * ================================================================== */

UsbSerial* UsbSerial::_instance = nullptr;

int8_t UsbSerial::_InitCb(void) {
    if (_instance) _instance->_connected = true;
    return 0;
}

int8_t UsbSerial::_DeInitCb(void) {
    if (_instance) _instance->_connected = false;
    return 0;
}

int8_t UsbSerial::_ControlCb(uint8_t cmd, uint8_t* pbuf, uint16_t length) {
    (void)length;
    switch (cmd) {
    case CDC_SET_LINE_CODING:
        // pbuf contains: uint32_t bitrate, uint8_t format, uint8_t paritytype, uint8_t datatype
        // Just accept it silently
        break;
    case CDC_GET_LINE_CODING:
        // Return a default line coding
        pbuf[0] = 0x80; // bps LSB
        pbuf[1] = 0x25;
        pbuf[2] = 0x00;
        pbuf[3] = 0x00; // 9600 bps
        pbuf[4] = 0x00; // 1 stop bit
        pbuf[5] = 0x00; // no parity
        pbuf[6] = 0x08; // 8 data bits
        break;
    case CDC_SET_CONTROL_LINE_STATE:
        // pbuf[0] bit 0 = DTR, bit 1 = RTS
        // Host asserted DTR = connected
        if (_instance) _instance->_connected = true;
        break;
    default:
        break;
    }
    return 0;
}

int8_t UsbSerial::_ReceiveCb(uint8_t* buf, uint32_t* len) {
    // Data from host — we don't process it for now
    // Just acknowledge reception
    (void)buf;
    (void)len;
    return 0;
}

/* ==================================================================
 * PCD (USB Peripheral) hardware initialization
 * ================================================================== */

static PCD_HandleTypeDef _hpcd;

extern "C" {

/**
 * @brief PCD -> HAL callback: data OUT stage complete
 */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
 * @brief PCD -> HAL callback: data IN stage complete
 */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
 * @brief PCD -> HAL callback: setup packet received
 */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t*)hpcd->Setup);
}

/**
 * @brief PCD -> HAL callback: SOF received
 */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData);
}

/**
 * @brief PCD -> HAL callback: reset received
 */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
    USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
    USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, speed);
}

/**
 * @brief PCD -> HAL callback: suspend
 */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
}

/**
 * @brief PCD -> HAL callback: resume
 */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData);
}

/**
 * @brief PCD -> HAL callback: ISO OUT incomplete
 */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

/**
 * @brief PCD -> HAL callback: ISO IN incomplete
 */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

/**
 * @brief PCD -> HAL callback: connect event
 */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData);
}

/**
 * @brief PCD -> HAL callback: disconnect event
 */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData);
}

} // extern "C"

/* ==================================================================
 * UsbSerial class implementation
 * ================================================================== */

UsbSerial::UsbSerial()
    : _connected(false)
{
    _instance = this;
    memset(&_usbd, 0, sizeof(_usbd));
    memset(&_hpcd, 0, sizeof(_hpcd));
    memset(_tx_buf, 0, sizeof(_tx_buf));
    
    // Setup CDC interface callbacks
    _cdc_if.Init     = _InitCb;
    _cdc_if.DeInit   = _DeInitCb;
    _cdc_if.Control  = _ControlCb;
    _cdc_if.Receive  = _ReceiveCb;
    
    // Setup descriptor callbacks
    _desc.GetDeviceDescriptor           = GetDeviceDescriptor;
    _desc.GetLangIDStrDescriptor        = GetLangIDStrDescriptor;
    _desc.GetManufacturerStrDescriptor  = GetManufacturerStrDescriptor;
    _desc.GetProductStrDescriptor       = GetProductStrDescriptor;
    _desc.GetSerialStrDescriptor        = GetSerialStrDescriptor;
    _desc.GetConfigurationStrDescriptor = GetConfigurationStrDescriptor;
    _desc.GetInterfaceStrDescriptor     = GetInterfaceStrDescriptor;
}

void UsbSerial::begin() {
    /* ---- Step 1: Enable USB clock ---- */
    __HAL_RCC_USB_CLK_ENABLE();
    
    /* ---- Step 2: PA12 as push-pull to drive USB DP pull-up (1.5kΩ to 3.3V) ---- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {};
    gpio.Pin   = GPIO_PIN_12;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    /* Pull PA12 LOW to enable USB DP pull-up (P-channel MOSFET circuit on BluePill) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    
    /* Small delay for host to detect pull-up */
    HAL_Delay(100);
    
    /* ---- Step 3: Configure PCD (USB peripheral) ---- */
    _hpcd.Instance = USB;
    _hpcd.Init.dev_endpoints = 8;
    _hpcd.Init.speed = PCD_SPEED_FULL;
    _hpcd.Init.low_power_enable = DISABLE;
    _hpcd.Init.lpm_enable = DISABLE;
    _hpcd.Init.battery_charging_enable = DISABLE;
    
    /* Link PCD <-> USBD (bidirectional) */
    _hpcd.pData = &_usbd;
    _usbd.pData = &_hpcd;
    
    /* ---- Step 4: Init USB Device stack (calls USBD_LL_Init -> HAL_PCD_Init) ---- */
    USBD_Init(&_usbd, &_desc, 0);
    USBD_RegisterClass(&_usbd, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&_usbd, &_cdc_if);
    USBD_Start(&_usbd);
}

void UsbSerial::println(const char* str) {
    if (!_connected) return;
    
    // Check if previous TX is still in progress
    if (_usbd.dev_state != USBD_STATE_CONFIGURED) return;
    
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)_usbd.pClassData;
    if (hcdc->TxState != 0) return; // Busy
    
    // Copy string + CRLF into temp buffer
    uint32_t len = 0;
    while (str[len] != '\0' && len < 62) { // 62 max to leave room for \r\n
        _tx_buf[len] = str[len];
        len++;
    }
    _tx_buf[len++] = '\r';
    _tx_buf[len++] = '\n';
    
    USBD_CDC_SetTxBuffer(&_usbd, _tx_buf, len);
    USBD_CDC_TransmitPacket(&_usbd);
}

void UsbSerial::println() {
    if (!_connected) return;
    if (_usbd.dev_state != USBD_STATE_CONFIGURED) return;
    
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)_usbd.pClassData;
    if (hcdc->TxState != 0) return;
    
    _tx_buf[0] = '\r';
    _tx_buf[1] = '\n';
    
    USBD_CDC_SetTxBuffer(&_usbd, _tx_buf, 2);
    USBD_CDC_TransmitPacket(&_usbd);
}

/* ==================================================================
 * USBD_LL_* Low-Level bridge functions (USB Core <-> HAL PCD)
 * ================================================================== */

extern "C" {

static PCD_HandleTypeDef* _pdev_to_hpcd(USBD_HandleTypeDef *pdev) {
    return (PCD_HandleTypeDef*)pdev->pData;
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev) {
    PCD_HandleTypeDef *hpcd = _pdev_to_hpcd(pdev);
    hpcd->pData = pdev;
    HAL_PCD_Init(hpcd);
    /* Enable USB IRQ */
    HAL_NVIC_SetPriority(USB_LP_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_IRQn);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
    HAL_PCD_DeInit(_pdev_to_hpcd(pdev));
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) {
    HAL_PCD_Start(_pdev_to_hpcd(pdev));
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) {
    HAL_PCD_Stop(_pdev_to_hpcd(pdev));
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_Open(_pdev_to_hpcd(pdev), ep_addr, ep_mps, ep_type);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_Close(_pdev_to_hpcd(pdev), ep_addr);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_Flush(_pdev_to_hpcd(pdev), ep_addr);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_SetStall(_pdev_to_hpcd(pdev), ep_addr);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_ClrStall(_pdev_to_hpcd(pdev), ep_addr);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    PCD_HandleTypeDef *hpcd = _pdev_to_hpcd(pdev);
    if ((ep_addr & 0x80) == 0x80) {
        return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
    } else {
        return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
    }
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) {
    HAL_StatusTypeDef hal = HAL_PCD_SetAddress(_pdev_to_hpcd(pdev), dev_addr);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_Transmit(_pdev_to_hpcd(pdev), ep_addr, pbuf, size);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size) {
    HAL_StatusTypeDef hal = HAL_PCD_EP_Receive(_pdev_to_hpcd(pdev), ep_addr, pbuf, size);
    return (hal == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    return HAL_PCD_EP_GetRxCount(_pdev_to_hpcd(pdev), ep_addr);
}

void USBD_LL_Delay(uint32_t Delay) {
    HAL_Delay(Delay);
}

} // extern "C"

/* ==================================================================
 * USB interrupt handler
 * ================================================================== */

extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) {
    HAL_PCD_IRQHandler(&_hpcd);
}