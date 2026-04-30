#pragma once

#include <cstdint>

/**
 * @brief Thin wrapper around TinyUSB CDC — handles all USB hardware
 *        initialisation, enumeration descriptors, and the FreeRTOS
 *        background task that pumps the TinyUSB state machine.
 *
 * Usage
 * -----
 *   1.  In main(), create the FreeRTOS task:
 *           xTaskCreate(USBSerial::task, "USB", 256, NULL,
 *                       configMAX_PRIORITIES - 1, NULL);
 *   2.  After the host enumerates the CDC port, call from any task:
 *           USBSerial::print("Hello\r\n");
 */
class USBSerial {
public:
    /**
     * @brief FreeRTOS task entry.
     *
     * Powers the USB peripheral, initialises TinyUSB, forces a bus
     * re-enumeration via the PA12 pull-up, then blocks on tud_task()
     * forever.
     */
    static void task(void *argument);

    /// @brief Returns true when the CDC serial port has been opened by the host.
    static bool isConnected();

    //--------------------------------------------------------------
    //  Output
    //--------------------------------------------------------------

    /// Write a NUL-terminated string (blocking if TinyUSB FIFO is full).
    static void print(const char *str);

    /// Write a NUL-terminated string followed by CR+LF.
    static void println(const char *str);

    /// Write exactly @p len bytes from @p data.
    static void write(const void *data, unsigned len);

    /// Flush the TinyUSB transmit FIFO.
    static void flush();

private:
    // TinyUSB descriptor callbacks (implemented in .cpp)
    static const uint8_t *deviceDescriptor();
    static const uint8_t *configDescriptor(uint8_t index);
    static const uint16_t *stringDescriptor(uint8_t index, uint16_t langid);
};