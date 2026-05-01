#pragma once

#include <cstdint>
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

/**
 * @brief Command-line size.  Matches the longest AT command or response
 *        we expect to handle.
 */
#define BLE_MAX_CMD_LEN   64

/**
 * @brief UART port selector for the JDY‑23 Bluetooth module.
 */
enum class BlePort : uint8_t {
    UART1 = 1,
    UART2 = 2,
    UART3 = 3,
};

// Forward-declare the HAL callbacks and all IRQ handlers with C linkage
// so that the friend declarations inside BleSerial match the extern "C"
// definitions in BleSerial.cpp.
extern "C" {
    void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
    void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
    void USART1_IRQHandler(void);
    void USART2_IRQHandler(void);
    void USART3_IRQHandler(void);
    void DMA1_Channel2_IRQHandler(void);
    void DMA1_Channel4_IRQHandler(void);
    void DMA1_Channel7_IRQHandler(void);
}

/**
 * @brief Non-blocking UART bridge for the JDY‑23 AT-command Bluetooth module.
 *
 * Supports USART1, USART2, or USART3 — selected at init time via begin().
 *
 * RX strategy : Interrupt‑driven, one byte at a time.
 *               Characters are assembled directly into cmd_buffer[].
 *               The FreeRTOS consumer task is woken via rx_semaphore
 *               when a \n- or \r‑terminated string arrives.
 *
 * TX strategy : DMA‑driven.
 *               A FreeRTOS binary semaphore blocks the calling task
 *               until the DMA transfer finishes.
 *
 * All members are static – the class is a namespace with private state.
 */
class BleSerial {
public:
    //------------------------------------------------------------------
    //  Public API
    //------------------------------------------------------------------

    /**
     * @brief  Initialise the USART, DMA, FreeRTOS primitives and arm
     *         the first RX interrupt.
     *
     * @param port  UART instance to use (UART1, UART2, or UART3).
     * @param baud  Baudrate for the JDY‑23  (e.g. 57600).
     *
     * Must be called exactly once, before any other method.
     */
    static void begin(BlePort port, uint32_t baud);

    /**
     * @brief  Blocking transmit of a NUL‑terminated string.
     *
     * Internally calls HAL_UART_Transmit_DMA and then blocks on the
     * TX binary semaphore.  Returns only when the entire string has
     * been shifted out by the DMA.
     */
    static void print(const char *str);

    //------------------------------------------------------------------
    //  Shared RX state  (ISR writes, task reads after rx_semaphore)
    //------------------------------------------------------------------
    static char                 cmd_buffer[BLE_MAX_CMD_LEN];
    static SemaphoreHandle_t    rx_semaphore;    ///< Binary semaphore – given by ISR

private:
    //------------------------------------------------------------------
    //  Hardware handles  (initialised by begin())
    //------------------------------------------------------------------
    static UART_HandleTypeDef   _huart;
    static DMA_HandleTypeDef    _hdma_tx;

    //------------------------------------------------------------------
    //  Active port tracking  (set by begin())
    //------------------------------------------------------------------
    static BlePort              _activePort;
    static USART_TypeDef       *_usartInstance;
    static DMA_Channel_TypeDef *_dmaChannel;

    //------------------------------------------------------------------
    //  FreeRTOS primitives
    //------------------------------------------------------------------
    static SemaphoreHandle_t    _tx_semaphore;   ///< Binary semaphore – taken during DMA

    //------------------------------------------------------------------
    //  RX assembly state  (updated from ISR)
    //------------------------------------------------------------------
    static uint8_t              _rx_byte;        ///< Single‑byte receive target for HAL
    static uint8_t              _rx_idx;

    //------------------------------------------------------------------
    //  Friends – match the extern "C" forward declarations above.
    //  The callback bodies and IRQ handlers (defined in BleSerial.cpp)
    //  access the private members through these friend grants.
    //------------------------------------------------------------------
    friend void ::HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
    friend void ::HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
    friend void ::USART1_IRQHandler(void);
    friend void ::USART2_IRQHandler(void);
    friend void ::USART3_IRQHandler(void);
    friend void ::DMA1_Channel2_IRQHandler(void);
    friend void ::DMA1_Channel4_IRQHandler(void);
    friend void ::DMA1_Channel7_IRQHandler(void);
};