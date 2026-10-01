/*
 * uart_it.c
 *
 *  Created on: Sep 12, 2024
 *      Author: ZS
 *
 */

////////////////////////////////////////////////////////////////////////////////
// define used uart interface for STDOUT&STDIN HERE
#define USEDUART         huart2
// uncomment line to enable echo of received chars
// #define UART_STDOUT_ECHO_ON
////////////////////////////////////////
#include "main.h"
/******************************************************************************/
void send_boot_message(void) {
  setbuf(stdout, NULL); // Ausgabe mit printf, putchar, ohne \n
  const char *msg_boot =
      "\r\n"
      "\r\n# reset #"
      "\r\n# %s"
      "\r\n# %s"
      "\r\n# HCLK = %.1f MHz"
      "\r\n"
      "\r\n";
  float MHZ = ((double) SystemCoreClock) / 1.0E6;
  printf(msg_boot, PRJ_VERSION, BUILD_DATE, MHZ);
}

/******************************************************************************/
extern UART_HandleTypeDef   USEDUART   ;
// UART_STDOUT - debug interface
#define UART_STDOUT       ( USEDUART )
char uartByteIn;
char uartBufferIn[2][UART_IN_BUF_SIZE] = { 0 }; // double buffer
int uartBufferToFill = 1;
int uartBufferReady = 0;
int uartBufferIndex = 0;
int uartBufferOverflowFlag = 0;
int uartCmdReady = 0;

/******************************************************************************/
/* Redirect printf, putchar ... functions to stdout --------------------------*/
int __io_putchar(int ch) {
  HAL_UART_Transmit(&UART_STDOUT, (uint8_t*) &ch, 1, HAL_MAX_DELAY);
  return ch;
}

/******************************************************************************/
void start_rx_uart_it(void) {
  HAL_UART_Receive(&UART_STDOUT, (uint8_t*) NULL, UART_IN_BUF_SIZE, 500);
  // flush garbage data from UART Data Register (see section 25.6.2 in rm0390)
  __HAL_UART_FLUSH_DRREGISTER(&UART_STDOUT);
  // start receiving in Interrupt Mode
  HAL_UART_Receive_IT(&UART_STDOUT, (uint8_t*) &uartByteIn, 1 );
  send_boot_message();
}

/******************************************************************************/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  // do nothing
}

/******************************************************************************/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  uint32_t retval;
  if ( huart == &UART_STDOUT ) {  //BEGIN UART_STDOUT
    if (uartBufferIndex >= ( UART_IN_BUF_SIZE - 1 ) ) {
      uartBufferIndex = 0;
      uartBufferOverflowFlag = 1;
    } else if (uartByteIn == '\r') {  		// line ends with carriage return '\r'
      uartBufferReady = uartBufferToFill;
      uartBufferIn[uartBufferReady][uartBufferIndex++] = 0;
      uartBufferIndex = 0;
      uartBufferToFill = (uartBufferToFill + 1) % 2;
      uartCmdReady = 1;
    } else if (uartByteIn == '\n') {
      // ignore newline
    } else { // save byte in BufferToFill
      uartBufferIn[uartBufferToFill][uartBufferIndex++] = uartByteIn;
#ifdef UART_STDOUT_ECHO_ON
      HAL_UART_Transmit(&UART_STDOUT, (uint8_t*) &uartByteIn, 1, 100);
#endif
    }
    // wait for next receive on uart2
    retval = HAL_UART_Receive_IT(&UART_STDOUT, (uint8_t*) &uartByteIn, 1 );

    if (retval != HAL_OK) {
      HAL_UART_Abort_IT(&UART_STDOUT);
      HAL_UART_Receive_IT(&UART_STDOUT, (uint8_t*) &uartByteIn, 1 );
    }
  } //END UART_STDOUT
}

/******************************************************************************/
void ClearBuffer(int buffer ) {
  for( int ix = 0; ix < UART_IN_BUF_SIZE; ix++ ) {
    uartBufferIn[buffer][ix] = 0;
  }
}
