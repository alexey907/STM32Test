#include "BleSerial.h"

#include <cstring>
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

//--------------------------------------------------------------------+
//  Static member definitions
//--------------------------------------------------------------------+

UART_HandleTypeDef   BleSerial::_huart1;
DMA_HandleTypeDef    BleSerial::_hdma_usart1_tx;

QueueHandle_t        BleSerial::_cmd_queue    = NULL;
SemaphoreHandle_t    BleSerial::_tx_semaphore = NULL;

uint8_t              BleSerial::_rx_byte = 0;
char                 BleSerial::_rx_buffer[BLE_MAX_CMD_LEN] = {0};
uint8_t              BleSerial::_rx_idx  = 0;

//--------------------------------------------------------------------+
//  Public API
//--------------------------------------------------------------------+

void BleSerial::begin(uint32_t baud) {
    // ---- FreeRTOS primitives ----
    // Queue: 5 slots, each BLE_MAX_CMD_LEN bytes (holds complete strings)
    _cmd_queue = xQueueCreate(5, BLE_MAX_CMD_LEN);
    configASSERT(_cmd_queue != NULL);

    // Binary semaphore: start "given" so first TX is unblocked
    _tx_semaphore = xSemaphoreCreateBinary();
    configASSERT(_tx_semaphore != NULL);
    xSemaphoreGive(_tx_semaphore);

    // ---- Clocks ----
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // ---- GPIO ----
    // PA9  = TX  : Alternate Function Push-Pull
    // PA10 = RX  : Input Floating
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin   = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin   = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOA, &gpio);

    // ---- HAL peripheral init ----
    {
        DMA_HandleTypeDef *hdma = &_hdma_usart1_tx;

        hdma->Instance = DMA1_Channel4;
        hdma->Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma->Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma->Init.MemInc              = DMA_MINC_ENABLE;
        hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma->Init.Mode                = DMA_NORMAL;
        hdma->Init.Priority            = DMA_PRIORITY_LOW;
        HAL_DMA_Init(hdma);
        __HAL_LINKDMA(&_huart1, hdmatx, *hdma);
    }
    {
        UART_HandleTypeDef *h = &_huart1;
        h->Instance          = USART1;
        h->Init.BaudRate     = baud;
        h->Init.WordLength   = UART_WORDLENGTH_8B;
        h->Init.StopBits     = UART_STOPBITS_1;
        h->Init.Parity       = UART_PARITY_NONE;
        h->Init.Mode         = UART_MODE_TX_RX;
        h->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
        h->Init.OverSampling = UART_OVERSAMPLING_16;
        HAL_UART_Init(h);
    }

    // ---- NVIC priorities (must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY) ----
    HAL_NVIC_SetPriority(USART1_IRQn,           5, 0);
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn,    5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    // ---- Arm the first RX interrupt ----
    HAL_UART_Receive_IT(&_huart1, &_rx_byte, 1);
}

void BleSerial::print(const char *str) {
    // Block until previous DMA transfer completes
    xSemaphoreTake(_tx_semaphore, portMAX_DELAY);

    HAL_UART_Transmit_DMA(&_huart1, (uint8_t *)str, (uint16_t)strlen(str));
}

bool BleSerial::commandAvailable() {
    return uxQueueMessagesWaiting(_cmd_queue) > 0;
}

void BleSerial::getCommand(char *out_str) {
    // Block until a line is available
    xQueueReceive(_cmd_queue, out_str, portMAX_DELAY);
}

//--------------------------------------------------------------------+
//  extern "C" IRQ handlers – route to HAL
//--------------------------------------------------------------------+

extern "C" void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&BleSerial::_huart1);
}

extern "C" void DMA1_Channel4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&BleSerial::_hdma_usart1_tx);
}

//--------------------------------------------------------------------+
//  extern "C" HAL callbacks  (ISR context)
//--------------------------------------------------------------------+
//  These MUST have C linkage so the linker resolves the symbol that
//  the HAL (weak) default expects.  The friends declared in BleSerial.h
//  grant these free functions access to the private static members.
//--------------------------------------------------------------------+

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART1) return;

    BaseType_t woken = pdFALSE;

    if (BleSerial::_rx_byte == '\n' || BleSerial::_rx_byte == '\r') {
        // Line terminator – push completed line into queue
        if (BleSerial::_rx_idx > 0) {
            BleSerial::_rx_buffer[BleSerial::_rx_idx] = '\0';
            xQueueSendFromISR(BleSerial::_cmd_queue, BleSerial::_rx_buffer, &woken);
            BleSerial::_rx_idx = 0;
        }
    } else {
        // Regular character – append to buffer if space remains
        if (BleSerial::_rx_idx < (BLE_MAX_CMD_LEN - 1)) {
            BleSerial::_rx_buffer[BleSerial::_rx_idx] = (char)BleSerial::_rx_byte;
            BleSerial::_rx_idx++;
        }
    }

    // Re-arm the one-byte receive interrupt
    HAL_UART_Receive_IT(&BleSerial::_huart1, &BleSerial::_rx_byte, 1);

    portYIELD_FROM_ISR(woken);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART1) return;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(BleSerial::_tx_semaphore, &woken);
    portYIELD_FROM_ISR(woken);
}