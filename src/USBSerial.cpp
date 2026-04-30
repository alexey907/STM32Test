#include "USBSerial.h"

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

//--------------------------------------------------------------------+
//  USB Descriptors
//--------------------------------------------------------------------+

static const char *string_desc_arr[] = {
    (const char[]){0x09, 0x04},   // 0: English (0x0409)
    "BoatTest",                    // 1: Manufacturer
    "BluePill CDC",                // 2: Product
    "123456",                      // 3: Serial Number
    "CDC Interface",               // 4: CDC interface string
};

// Forward to the C-linkage thunks below
extern "C" {
    uint8_t const *tud_descriptor_device_cb(void);
    uint8_t const *tud_descriptor_configuration_cb(uint8_t index);
    uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);
    void USB_LP_CAN1_RX0_IRQHandler(void);
}

//--------------------------------------------------------------------+
//  C-linkage TinyUSB callbacks
//--------------------------------------------------------------------+

extern "C" uint8_t const *tud_descriptor_device_cb(void) {
    static const tusb_desc_device_t desc_device = {
        .bLength         = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB          = 0x0200,
        .bDeviceClass    = TUSB_CLASS_MISC,
        .bDeviceSubClass = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0 = 64,
        .idVendor        = 0x0483,
        .idProduct       = 0x5740,
        .bcdDevice       = 0x0100,
        .iManufacturer   = 0x01,
        .iProduct        = 0x02,
        .iSerialNumber   = 0x03,
        .bNumConfigurations = 1,
    };
    return (uint8_t const *)&desc_device;
}

extern "C" uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    static const uint8_t desc_configuration[] = {
        TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN,
                              TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
        TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),
    };
    return desc_configuration;
}

extern "C" uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t str_desc_buf[32];
    uint8_t count;

    if (index == 0) {
        memcpy(&str_desc_buf[1], string_desc_arr[0], 2);
        count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
            return NULL;

        const char *str = string_desc_arr[index];
        count = (uint8_t)strlen(str);

        if (count > 31) count = 31;

        for (uint8_t i = 0; i < count; i++) {
            str_desc_buf[1 + i] = str[i];
        }
    }

    str_desc_buf[0] = (uint16_t)((uint16_t)(TUSB_DESC_STRING << 8) |
                                 (uint16_t)(2 * count + 2));

    return str_desc_buf;
}

//--------------------------------------------------------------------+
//  STM32 USB interrupt handler
//--------------------------------------------------------------------+

extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) {
    tud_int_handler(0);
}

//--------------------------------------------------------------------+
//  USBSerial public methods
//--------------------------------------------------------------------+

void USBSerial::task(void *argument) {
    (void)argument;

    // 1. FreeRTOS-compatible priority grouping (required by STM32 HAL)
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    // 2. Power on USB peripheral clock
    __HAL_RCC_USB_CLK_ENABLE();

    // 3. Arm the TinyUSB state machine and interrupt
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    tusb_init();

    // 4. Force re-enumeration via PA12 (Blue Pill's hardwired 1.5k pull-up)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOA, &gpio);

    // 5. Pump the TinyUSB state machine forever
    for (;;) {
        tud_task();
    }
}

bool USBSerial::isConnected() {
    return tud_cdc_connected();
}

void USBSerial::print(const char *str) {
    if (!tud_cdc_connected()) return;
    tud_cdc_write_str(str);
    tud_cdc_write_flush();
}

void USBSerial::println(const char *str) {
    if (!tud_cdc_connected()) return;
    tud_cdc_write_str(str);
    tud_cdc_write_str("\r\n");
    tud_cdc_write_flush();
}

void USBSerial::write(const void *data, unsigned len) {
    if (!tud_cdc_connected()) return;
    tud_cdc_write(data, len);
}

void USBSerial::flush() {
    if (!tud_cdc_connected()) return;
    tud_cdc_write_flush();
}