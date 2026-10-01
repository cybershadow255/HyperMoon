/*
 * commands.c
 *
 *  Created on: Nov 27, 2024
 *      Author: ZS
 */

#include "main.h"

/*      Command Map array       */
// add new "Command" strings with &function_pointers here
const CommandMap_t g_cmdMap[] = {
    { "LED",        &cmd_LED              },
    { "PC3",        &cmd_PC3              },
    { "BLINK",      &cmd_blink_LED        },
    { "MEAS",       &cmd_meas_out         },
    { "PSC",        &cmd_psc              },
    { "ARR",        &cmd_arr              },
    { "CCR1",       &cmd_ccr1             },
    { "TARG",       &cmd_test_args        },
    { "HELP", 		&cmd_Help 			  },
    { "VERS",       &cmd_Print_Version    }
};
// add function command prototypes to header file "commands.h"
// specify max number of arguments for user commands here
const int NUM_USER_ARGS = 3;
///////////////////////////////////////////////////////////
const int MAX_NUM_ARGS = NUM_USER_ARGS + 1;
const int NUM_COMMANDS = (sizeof(g_cmdMap) / sizeof(CommandMap_t));

/***********************************************************/
void cmd_Help(char **Args, int argCount) {
  printf("available Commands with Arguments \r\n");
  printf("HELP \r\n");
  printf("LED [ON|OFF]\r\n");
  printf("PC3 [ON|OFF|TOGGLE]\r\n");
  printf("BLINK [ON|OFF]\r\n");
  printf("MEAS [ON|OFF] [uint]\r\n");
  printf("PSC  [uint]\r\n");
  printf("ARR  [uint]\r\n");
  printf("CCR1 [uint]\r\n");
  printf("VERS       \r\n");
  printf("TARG [ARG1] [ARG2] [ARG3] \r\n");
  printf("\r\n");
}

/***********************************************************/
void cmd_test_args(char **Args, int argCount) {
  printf("argCount = %d \r\n", argCount);
  for (int i = 0; i <= argCount; i++) {
    printf(" Args[%d] = \"%s\" \r\n", i, Args[i]);
  }
  printf("\r\n");
}

/***********************************************************/
void cmd_Print_Version(char **Args, int argCount) {
  const char *msg =
      "%s \r\n"
      "%s \r\n"
      "\r\n";
  printf(msg, PRJ_VERSION, BUILD_DATE);
}

/***********************************************************/
void cmd_psc(char **Args, int argCount) {
  switch (argCount) {
  case 0:
    printf("act PSC = %lu \r\n", TIM4->PSC);
    break;
  case 1: {
    uint32_t psc = atoi(Args[1]);
    TIM4->PSC = psc;
    printf("new PSC = %lu \r\n", TIM4->PSC);
    break;
  }
  default:
    printf("syntax error\r\n");
  }
}

/***********************************************************/
void cmd_arr(char **Args, int argCount) {
  switch (argCount) {
  case 0:
    printf("act ARR = %lu \r\n", TIM4->ARR);
    break;
  case 1: {
    uint32_t arr = atoi(Args[1]);
    TIM4->ARR = arr;
    printf("new ARR = %lu \r\n", TIM4->ARR);
    break;
  }
  default:
    printf("error\r\n");
  }
}

/***********************************************************/
void cmd_ccr1(char **Args, int argCount) {
  switch (argCount) {
  case 0:
    printf("act CCR1 = %lu \r\n", TIM4->CCR1);
    break;
  case 1: {
    uint32_t ccr1 = atoi(Args[1]);
    TIM4->CCR1 = ccr1;
    printf("new CCR1 = %lu \r\n", TIM4->CCR1);
    break;
  }
  default:
    printf("error\r\n");
  }
}

/***********************************************************/
void Set_LED(GPIO_PinState State) {
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, State);
}

GPIO_PinState Get_LED_state(void) {
  return (HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin));
}

/***********************************************************/
void cmd_LED(char **Args, int argCount) {
  GPIO_PinState state = Get_LED_state();
  switch (argCount) {
  case 0: {
    char *ledstate = (GPIO_PIN_RESET == state) ? "OFF" : "ON";
    printf("LED is '%s'\r\n", ledstate);
    break;
  }
  case 1: {
    bool validSetCommand = false;
    if (0 == strcmp(Args[1], "ON")) {
      state = GPIO_PIN_SET;
      validSetCommand = true;
    } else if (0 == strcmp(Args[1], "OFF")) {
      state = GPIO_PIN_RESET;
      validSetCommand = true;
    }

    if (true == validSetCommand) {
      Set_LED(state);
      printf("LED set to %s \r\n", Args[1]);
      break;
    }
  }
  default:
    printf("error\r\n");
  }
}

/***********************************************************/
void Set_PC3(GPIO_PinState State) {
  HAL_GPIO_WritePin(PC3_GPIO_Port, PC3_Pin, State);
}

GPIO_PinState Get_PC3_state(void) {
  return (HAL_GPIO_ReadPin(PC3_GPIO_Port, PC3_Pin));
}

void cmd_PC3(char **Args, int argCount) {
  GPIO_PinState state = Get_PC3_state();
  switch (argCount) {
  case 0: {
    char *pc3state = (GPIO_PIN_RESET == state) ? "OFF" : "ON";
    printf("PC3 is '%s'\r\n", pc3state);
    break;
  }
  case 1:
    if (0 == strcmp(Args[1], "ON")) {
      Set_PC3(GPIO_PIN_SET);
      printf("PC3 set to ON \r\n");
    } else if (0 == strcmp(Args[1], "OFF")) {
      Set_PC3(GPIO_PIN_RESET);
      printf("PC3 set to OFF \r\n");
    } else if (0 == strcmp(Args[1], "TOGGLE")) {
      HAL_GPIO_TogglePin(PC3_GPIO_Port, PC3_Pin);
      state = Get_PC3_state();
      char *newstate = (GPIO_PIN_RESET == state) ? "OFF" : "ON";
      printf("PC3 toggled to %s \r\n", newstate);
    } else {
      printf("error\r\n");
    }
    break;
  default:
    printf("error\r\n");
  }
}

/***********************************************************/
void cmd_blink_LED(char **Args, int argCount) {

  if (0 == strcmp(Args[1], "ON")) {
    tick_BLINKLED = HAL_GetTick();
    en_blinkled = true;
    printf("Blinking LED started\r\n");
  } else if (0 == strcmp(Args[1], "OFF")) {
    en_blinkled = false;
    printf("Blinking LED stopped\r\n");
  }
}

/***********************************************************/
void cmd_meas_out(char **Args, int argCount) {

  if (0 == strcmp(Args[1], "ON")) {
    tick_MEASOUT = HAL_GetTick();
    en_meas_out = true;
    printf("Measurements printing started\r\n");
  } else if (0 == strcmp(Args[1], "OFF")) {
    en_meas_out = false;
    printf("Measurements printing stopped\r\n");
  } else {
    if ( NULL != Args[1] ) {
      char_txtout = Args[1][0] % 128;
    }
  }
}
