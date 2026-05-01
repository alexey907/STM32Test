#include "BleSerial.h"

#include <cstring>
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

//--------------------------------------------------------------------+
//  Static member definitions  (private)
//--------------------------------------------------------------------+

UART_HandleTypeDef   BleSerial::_huart;
DMA_HandleTypeDef    BleSerial::_hdma_tx;

BlePort              BleSerial::_activePort    = BlePort::UART1;
USART_TypeDef       *BleSerial::_usartInstance = USART1;
DMA_Channel_TypeDef *BleSerial::_dmaChannel    = DMA1_Channel4;

SemaphoreHandle_t    BleSerial::_tx_semaphore = NULL;

uint8_t              BleSerial::_rx_byte = 0;
uint8_t              BleSerial::_rx_idx  = 0;

//--------------------------------------------------------------------+
//  Static member definitions  (public – shared with app task)
//--------------------------------------------------------------------+

char                 BleSerial::cmd_buffer[BLE_MAX_CMD_LEN] = {0};
SemaphoreHandle_t    BleSerial::rx_semaphore = NULL;

//--------------------------------------------------------------------+
//  Public API
//--------------------------------------------------------------------+

void BleSerial::begin(BlePort port, uint32_t baud) {
    _activePort = port;

    // ---- FreeRTOS primitives ----
    _tx_semaphore = xSemaphoreCreateBinary();
    configASSERT(_tx_semaphore != NULL);
    xSemaphoreGive(_tx_semaphore);

    rx_semaphore = xSemaphoreCreateBinary();
    configASSERT(rx_semaphore != NULL);

    // ---- Select hardware resources per port ----
    GPIO_TypeDef    *gpioPort  = NULL;
    uint16_t         txPin     = 0;
    uint16_t         rxPin     = 0;
    IRQn_Type        usartIrq  = USART1_IRQn;
    IRQn_Type        dmaIrq    = DMA1_Channel4_IRQn;

    switch (port) {
    case BlePort::UART1:
        _usartInstance = USART1;
        _dmaChannel    = DMA1_Channel4;
        gpioPort       = GPIOA;
        txPin          = GPIO_PIN_9;
        rxPin          = GPIO_PIN_10;
        usartIrq       = USART1_IRQn;
        dmaIrq         = DMA1_Channel4_IRQn;
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        break;

    case BlePort::UART2:
        _usartInstance = USART2;
        _dmaChannel    = DMA1_Channel7;
        gpioPort       = GPIOA;
        txPin          = GPIO_PIN_2;
        rxPin          = GPIO_PIN_3;
        usartIrq       = USART2_IRQn;
        dmaIrq         = DMA1_Channel7_IRQn;
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        break;

    case BlePort::UART3:
        _usartInstance = USART3;
        _dmaChannel    = DMA1_Channel2;
        gpioPort       = GPIOB;
        txPin          = GPIO_PIN_10;
        rxPin          = GPIO_PIN_11;
        usartIrq       = USART3_IRQn;
        dmaIrq         = DMA1_Channel2_IRQn;
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        break;
    }

    // ---- Clocks (DMA is common) ----
    __HAL_RCC_DMA1_CLK_ENABLE();

    // ---- GPIO ----
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin   = txPin;
    HAL_GPIO_Init(gpioPort, &gpio);

    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin   = rxPin;
    HAL_GPIO_Init(gpioPort, &gpio);

    // ---- HAL peripheral init ----
    {
        DMA_HandleTypeDef *hdma = &_hdma_tx;

        hdma->Instance = _dmaChannel;
        hdma->Init.Direction           = DMA_MEMORY_TO_PERIPH;
        hdma->Init.PeriphInc           = DMA_PINC_DISABLE;
        hdma->Init.MemInc              = DMA_MINC_ENABLE;
        hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
        hdma->Init.Mode                = DMA_NORMAL;
        hdma->Init.Priority            = DMA_PRIORITY_LOW;
        HAL_DMA_Init(hdma);
        __HAL_LINKDMA(&_huart, hdmatx, *hdma);
    }
    {
        UART_HandleTypeDef *h = &_huart;
        h->Instance          = _usartInstance;
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
    HAL_NVIC_SetPriority(usartIrq,  5, 0);
    HAL_NVIC_SetPriority(dmaIrq,    5, 0);
    HAL_NVIC_EnableIRQ(usartIrq);
    HAL_NVIC_EnableIRQ(dmaIrq);

    // ---- Arm the first RX interrupt ----
    HAL_UART_Receive_IT(&_huart, &_rx_byte, 1);
}

void BleSerial::print(const char *str) {
    xSemaphoreTake(_tx_semaphore, portMAX_DELAY);
    HAL_UART_Transmit_DMA(&_huart, (uint8_t *)str, (uint16_t)strnlen(str, 256));
}

//--------------------------------------------------------------------+
//  extern "C" IRQ handlers – route to HAL for the active port
//--------------------------------------------------------------------+

extern "C" void USART1_IRQHandler(void) {
    if (BleSerial::_activePort == BlePort::UART1)
        HAL_UART_IRQHandler(&BleSerial::_huart);
}

extern "C" void USART2_IRQHandler(void) {
    if (BleSerial::_activePort == BlePort::UART2)
        HAL_UART_IRQHandler(&BleSerial::_huart);
}

extern "C" void USART3_IRQHandler(void) {
    if (BleSerial::_activePort == BlePort::UART3)
        HAL_UART_IRQHandler(&BleSerial::_huart);
}

extern "C" void DMA1_Channel2_IRQHandler(void) {
    if (BleSerial::_activePort == BlePort::UART3)
        HAL_DMA_IRQHandler(&BleSerial::_hdma_tx);
}

extern "C" void DMA1_Channel4_IRQHandler(void) {
    if (BleSerial::_activePort == BlePort::UART1)
        HAL_DMA_IRQHandler(&BleSerial::_hdma_tx);
}

extern "C" void DMA1_Channel7_IRQHandler(void) {
    if (BleSerial::_activePort == BlePort::UART2)
        HAL_DMA_IRQHandler(&BleSerial::_hdma_tx);
}

//--------------------------------------------------------------------+
//  extern "C" HAL callbacks  (ISR context)
//--------------------------------------------------------------------+

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != BleSerial::_usartInstance) return;

    BaseType_t woken = pdFALSE;

    if (BleSerial::_rx_byte == '\n' || BleSerial::_rx_byte == '\r') {
        if (BleSerial::_rx_idx > 0) {
            BleSerial::cmd_buffer[BleSerial::_rx_idx] = '\0';
            BleSerial::_rx_idx = 0;
            xSemaphoreGiveFromISR(BleSerial::rx_semaphore, &woken);
        }
    } else {
        if (BleSerial::_rx_idx < (BLE_MAX_CMD_LEN - 1)) {
            BleSerial::cmd_buffer[BleSerial::_rx_idx] = (char)BleSerial::_rx_byte;
            BleSerial::_rx_idx++;
        }
    }

    HAL_UART_Receive_IT(&BleSerial::_huart, &BleSerial::_rx_byte, 1);
    portYIELD_FROM_ISR(woken);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != BleSerial::_usartInstance) return;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(BleSerial::_tx_semaphore, &woken);
    portYIELD_FROM_ISR(woken);
}