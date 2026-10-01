/*
 * uart_it.h
 *
 *  Created on: Sep 12, 2024
 *      Author: ZS
 */

#ifndef INC_UART_IT_H_
#define INC_UART_IT_H_

#include <stdio.h>

#define UART_IN_BUF_SIZE  40

extern char uartByteIn;
extern char uartBufferIn[2][UART_IN_BUF_SIZE]; // double buffer
extern int  uartBufferToFill;
extern int  uartBufferReady;
extern int  uartBufferIndex;
extern int  uartBufferOverflowFlag;
extern int  uartCmdReady;

void start_rx_uart_it(void);
void send_boot_message(void);
void ClearBuffer(int buffer );

#endif /* INC_UART_IT_H_ */
