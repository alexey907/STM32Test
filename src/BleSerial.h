#pragma once

#include <cstdint>
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/**
 * @brief Command-line size.  Matches the longest AT command or response
 *        we expect to handle.
 */
#define BLE_MAX_CMD_LEN   64

// Forward-declare the HAL callbacks and IRQ handlers with C linkage so
// that the friend declarations inside BleSerial match the extern "C"
// definitions in BleSerial.cpp.
extern "C" {
    void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
    void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
    void USART1_IRQHandler(void);
    void DMA1_Channel4_IRQHandler(void);
}

/**
 * @brief Non-blocking UART bridge for the JDY‑23 AT-command Bluetooth module.
 *
 * Hardware : STM32F103 – USART1  (PA9 = TX,  PA10 = RX)
 *
 * RX strategy : Interrupt‑driven, one byte at a time.
 *               Characters are assembled into a line buffer.
 *               The FreeRTOS consumer task is only woken when a complete
 *               \n- or \r‑terminated string arrives.
 *
 * TX strategy : DMA‑driven  (DMA1 Channel 4).
 *               A FreeRTOS binary semaphore blocks the calling task until
 *               the DMA transfer finishes.
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
     * @param baud  Baudrate for the JDY‑23  (e.g. 57600).
     *
     * Must be called exactly once, before any other method.
     */
    static void begin(uint32_t baud);

    /**
     * @brief  Blocking transmit of a NUL‑terminated string.
     *
     * Internally calls HAL_UART_Transmit_DMA and then blocks on the
     * TX binary semaphore.  Returns only when the entire string has
     * been shifted out by the DMA.
     */
    static void print(const char *str);

    /**
     * @brief  Non‑blocking poll: has a complete line arrived?
     *
     * @retval true   One or more terminated lines are waiting in the queue.
     * @retval false  No complete line is available yet.
     */
    static bool commandAvailable();

    /**
     * @brief  Dequeue the oldest completed line.
     *
     * @param out_str  Caller‑supplied buffer of at least BLE_MAX_CMD_LEN
     *                 bytes.  The dequeued string is NUL‑terminated.
     *
     * @note  Call commandAvailable() before getCommand() – this function
     *        blocks indefinitely if the queue is empty.
     */
    static void getCommand(char *out_str);

private:
    //------------------------------------------------------------------
    //  Hardware handles  (initialised by begin())
    //------------------------------------------------------------------
    static UART_HandleTypeDef   _huart1;
    static DMA_HandleTypeDef    _hdma_usart1_tx;

    //------------------------------------------------------------------
    //  FreeRTOS primitives
    //------------------------------------------------------------------
    static QueueHandle_t        _cmd_queue;      ///< Holds BLE_MAX_CMD_LEN‑byte strings
    static SemaphoreHandle_t    _tx_semaphore;   ///< Binary semaphore – taken during DMA

    //------------------------------------------------------------------
    //  RX assembly state  (updated from ISR)
    //------------------------------------------------------------------
    static uint8_t              _rx_byte;        ///< Single‑byte receive target for HAL
    static char                 _rx_buffer[BLE_MAX_CMD_LEN];
    static uint8_t              _rx_idx;

    //------------------------------------------------------------------
    //  Friends – match the extern "C" forward declarations above.
    //  The callback bodies and IRQ handlers (defined in BleSerial.cpp)
    //  access the private members through these friend grants.
    //------------------------------------------------------------------
    friend void ::HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
    friend void ::HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
    friend void ::USART1_IRQHandler(void);
    friend void ::DMA1_Channel4_IRQHandler(void);
};
