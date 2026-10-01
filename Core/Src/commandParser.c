/*
 * commandParser.c
 *
 *  Created on: Nov 27, 2024
 *      Author: ZS
 */

#include "main.h"

// Functions
/***********************************************************/
void Check_Command(void) {
  if ( 1 == uartBufferOverflowFlag ) {
    printf("\r\nError: Input Buffer overflow\r\n");
    uartBufferOverflowFlag = 0;
    return;
  }
  if (1 == uartCmdReady) { // new commandline entered
    bool cmdfound = Process_Command(uartBufferIn[uartBufferReady]);
    if (!cmdfound)
      printf("Error: Unknown Command >%s<\r\n",
          uartBufferIn[uartBufferReady]);
    ClearBuffer(uartBufferReady);
    uartCmdReady = 0;      // reset flag !!!
  }
}

/***********************************************************/
bool Process_Command(char *SerialBuffer) {
  char lbuf[UART_IN_BUF_SIZE] = {0};
  for (int ix = 0; ix < UART_IN_BUF_SIZE; ix++ ) {
    lbuf[ix] = toupper(SerialBuffer[ix]);
  }
  bool commandFound = false;
  char *token = strtok(lbuf, " ;");
  char *args[MAX_NUM_ARGS];
  int argCount = 0;

  while ((NULL != token) && (MAX_NUM_ARGS > argCount)) {
    args[argCount++] = token;
    token = strtok(NULL, " ;");
  }

  if ( 0 == argCount ) { // handle CR
    printf("\r\n");
    commandFound = true;
  } else
  // find the command string within the command_map array
  for (int cmdIndex = 0; cmdIndex < NUM_COMMANDS; ++cmdIndex) {
    if (0 == strcmp(args[0], g_cmdMap[cmdIndex].cmdName)) {
      // call the appropriate cmd function
      g_cmdMap[cmdIndex].funcPtr(&args[0],argCount-1);
      commandFound = true;
      break;
    }
  }
  return (commandFound);
}
