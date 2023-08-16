#ifndef __UART_H__
#define __UART_H__

#include "uart_hw.h"

// Receive callback function signature
typedef void (*UartRecv_cb)(char *buf, short len);

// Initialize UARTs to the provided baud rates (115200 recommended). This also makes the os_printf
// calls use uart1 for output (for debugging purposes)
void uart_init(uint32 conf0, UartBautRate uart0_br, UartBautRate uart1_br);

// Add a receive callback function, this is called on the uart receive task each time a chunk
// of bytes are received. A small number of callbacks can be added and they are all called
// with all new characters.
void uart_add_recv_cb(UartRecv_cb cb);

#endif /* __UART_H__ */
