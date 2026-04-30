#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb_config.h"
#include "tusb.h"

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Configure HSE oscillator
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;  // 8MHz * 9 = 72MHz
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // Configure clock tree
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    // USB clock: 72MHz PLL / 1.5 = 48MHz required for USB Full-Speed
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

// TinyUSB Background Task — processes USB state machine
void UsbDeviceTask(void *argument) {
    (void)argument;

    // 1. Force Windows to re-enumerate — Blue Pill has a hardwired D+ pull-up.
    //    Driving PA12 low for 10ms drops the USB connection so the host sees a
    //    clean disconnect, then releasing it starts a fresh handshake.
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio.Mode = GPIO_MODE_INPUT;   // Release D+ back to high-Z
    HAL_GPIO_Init(GPIOA, &gpio);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. Lower USB interrupt priority so FreeRTOS API calls are legal inside ISR.
    //    STM32 NVIC defaults to priority 0 (max); FreeRTOS requires ≤ configMAX_SYSCALL_INTERRUPT_PRIORITY (5).
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);

    // 3. Initialize TinyUSB stack
    tusb_init();

    for (;;) {
        tud_task();   // tud_task_ext(INFINITE, false) — blocks until event arrives
    }
}

// Blink Task — toggles LED and prints "Led Blink" over USB Serial
void StartBlinkTask(void *argument) {
    (void)argument;
    for (;;) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_2);

        if (tud_cdc_connected()) {
            tud_cdc_write_str("Led Blink\r\n");
            tud_cdc_write_flush();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    // TinyUSB needs its own task to run tud_task()
    xTaskCreate(UsbDeviceTask, "USB", 512, NULL, configMAX_PRIORITIES - 1, NULL);

    // Blink task
    xTaskCreate(StartBlinkTask, "Blink", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

//--------------------------------------------------------------------+
// TinyUSB Device Descriptors
//--------------------------------------------------------------------+

// String descriptor table: index 0 = language, 1 = manufacturer, 2 = product, 3 = serial
static const char *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },   // 0: English (0x0409)
    "BoatTest",                       // 1: Manufacturer
    "BluePill CDC",                   // 2: Product
    "123456",                         // 3: Serial Number
    "CDC Interface",                  // 4: CDC interface string (used by TUD_CDC_DESCRIPTOR)
};

// Device descriptor
uint8_t const *tud_descriptor_device_cb(void) {
    static const tusb_desc_device_t desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,      // USB 2.0
        .bDeviceClass       = TUSB_CLASS_MISC,
        .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol    = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0    = 64,
        .idVendor           = 0xCAFE,
        .idProduct          = 0x4001,
        .bcdDevice          = 0x0100,
        .iManufacturer      = 0x01,
        .iProduct           = 0x02,
        .iSerialNumber      = 0x03,
        .bNumConfigurations = 1,
    };
    return (uint8_t const *)&desc_device;
}

// Configuration descriptor (CDC)
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    static const uint8_t desc_configuration[] = {
        // Config number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN,
                              TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
        // Interface number, string index, EP notification address/size, EP data address (out, in) and size
        TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),
    };
    return desc_configuration;
}

// String descriptor
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
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

    str_desc_buf[0] = (uint16_t)((uint16_t)(TUSB_DESC_STRING << 8) | (uint16_t)(2 * count + 2));

    return str_desc_buf;
}

// STM32 USB interrupt handler — routes hardware IRQ into TinyUSB
extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) {
    tud_int_handler(0);
}

void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}